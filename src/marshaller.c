#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>

#include "json.h"
#include "marshaller.h"

void _marshallPanic(const char* name, const char* reason) {
	if (reason == NULL) {
		reason = strerror(errno);
	}

	fprintf(stderr, "panic: marshaller (%s): %s\n", name, reason);
	exit(2);
}

static struct marshaller {
	const char* name;
	size_t size;
	jsonValue_t* (*marshaller)(void*);
	void* (*unmarshaller)(jsonValue_t*);
	void (*free)(void*, bool);
}* marshallerList = NULL;
static size_t marshallerListLength = 0;

static struct marshaller* findMarshaller(const char* type) {
	for (size_t i = 0; i < marshallerListLength; i++) {
		if (strcmp(type, marshallerList[i].name) == 0) {
			return &marshallerList[i];
		}
	}
	
	return NULL;
}

void _registerMarshaller(int namesCount, const char** names, size_t size, jsonValue_t* (*marshaller)(void*),  void* (*unmarshaller)(jsonValue_t*), void (*structFree)(void*, bool)) {
	marshallerList = realloc(marshallerList, (sizeof(struct marshaller)) * (marshallerListLength + namesCount));
	if (marshallerList == NULL) {
		_marshallPanic(names[0], NULL);
	}
	
	for (int i = 0; i < namesCount; i++) {
		if (findMarshaller(names[i])) {
			_marshallPanic(names[i], "marshaller for name already present");
		}
	
		marshallerList[marshallerListLength++] = (struct marshaller) {
			.name = names[i],
			.size = size,
			.marshaller = marshaller,
			.unmarshaller = unmarshaller,
			.free = structFree
		};
	}
}

static jsonValue_t* json_marshall_char(void* value) {
	return json_long(*((char*) value));
}

static jsonValue_t* json_marshall_short(void* value) {
	return json_long(*((short*) value));
}

static jsonValue_t* json_marshall_int(void* value) {
	return json_long(*((int*) value));
}

static jsonValue_t* json_marshall_long(void* value) {
	return json_long(*((long*) value));
}

static jsonValue_t* json_marshall_long_long(void* value) {
	return json_long(*((long long*) value));
}

static jsonValue_t* json_marshall_float(void* value) {
	return json_double(*((float*) value));
}

static jsonValue_t* json_marshall_double(void* value) {
	return json_double(*((double*) value));
}

static jsonValue_t* json_marshall_string(void* value) {
	return json_string((const char*) value);
}

static jsonValue_t* json_marshall_bool(void* value) {
	return json_bool(*((bool*) value));
}

jsonValue_t* _json_marshall_array_value(const char* type, void* value) {
	if (value == NULL) {
		return json_null();
	}

	size_t size;
	for (size = 0; *(((void**) value) + size) != NULL; size++);
	
	jsonValue_t** array = alloca(size * sizeof(jsonValue_t*));
	
	for(size_t i = 0; i < size; i++) {
		array[i] = _json_marshall_value(type, *(((void**) value) + i));
		if (array[i] == NULL) {
			return NULL;
		}
	}
	
	return json_array_direct(true, size, array);
}

jsonValue_t* _json_marshall_value(const char* type, void* value) {
	if (value == NULL) {
		return json_null();
	} else if (strcmp(type, "char") == 0) {
		return json_marshall_char(value);
	} else if (strcmp(type, "short") == 0) {
		return json_marshall_short(value);
	} else if (strcmp(type, "int") == 0) {
		return json_marshall_int(value);
	} else if (strcmp(type, "long") == 0) {
		return json_marshall_long(value);
	} else if (strcmp(type, "long long") == 0) {
		return json_marshall_long_long(value);
	} else if (strcmp(type, "float") == 0) {
		return json_marshall_float(value);
	} else if (strcmp(type, "double") == 0) {
		return json_marshall_double(value);
	} else if (strcmp(type, "string") == 0) {
		return json_marshall_string(value);
	} else if (strcmp(type, "bool") == 0) {
		return json_marshall_bool(value);	
	} else {
		struct marshaller* marshaller = findMarshaller(type);
		if (marshaller == NULL) {
			_marshallPanic(type, "unknown type");
		}
		return marshaller->marshaller(value);
	}
}
char* _json_marshall(const char* type, void* value) {
	jsonValue_t* json = _json_marshall_value(type, value);
	if (json == NULL)
		return NULL;
		
	char* result = json_stringify(json);
	json_free(json);

	return result;
}

char* _json_marshall_array(const char* type, void* value) {
	jsonValue_t* json = _json_marshall_array_value(type, value);
	if (json == NULL)
		return NULL;
		
	char* result = json_stringify(json);
	json_free(json);

	return result;
}

static void* json_unmarshall_char(jsonValue_t* value) {
	if (value->type != JSON_LONG)
		return NULL;

	char* tmp = malloc(sizeof(char));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_short(jsonValue_t* value) {
	if (value->type != JSON_LONG)
		return NULL;

	short* tmp = malloc(sizeof(short));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_int(jsonValue_t* value) {
	if (value->type != JSON_LONG)
		return NULL;

	int* tmp = malloc(sizeof(int));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_long(jsonValue_t* value) {
	if (value->type != JSON_LONG)
		return NULL;

	long* tmp = malloc(sizeof(long));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_long_long(jsonValue_t* value) {
	if (value->type != JSON_LONG)
		return NULL;

	long long* tmp = malloc(sizeof(long long));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_float(jsonValue_t* value) {
	if (value->type != JSON_DOUBLE && value->type != JSON_LONG)
		return NULL;

	float* tmp = malloc(sizeof(float));
	if (tmp == NULL)
		return NULL;
		
	if (value->type == JSON_DOUBLE)
		*tmp = value->value.real;
	else
		*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_double(jsonValue_t* value) {
	if (value->type != JSON_DOUBLE && value->type != JSON_LONG)
		return NULL;

	double* tmp = malloc(sizeof(double));
	if (tmp == NULL)
		return NULL;
		
	if (value->type == JSON_DOUBLE)
		*tmp = value->value.real;
	else
		*tmp = value->value.integer;
	return tmp;
}

static void* json_unmarshall_bool(jsonValue_t* value) {
	if (value->type != JSON_BOOL)
		return NULL;

	bool* tmp = malloc(sizeof(bool));
	if (tmp == NULL)
		return NULL;
		
	*tmp = value->value.boolean;
	return tmp;
}

static void* json_unmarshall_string(jsonValue_t* value) {
	if (value->type != JSON_STRING)
		return NULL;

	char* tmp = strdup(value->value.string);
	
	return tmp;
}

void* _json_unmarshall_array_value(const char* type, jsonValue_t* value) {
	if (value->type != JSON_ARRAY)
		return NULL;
		
	size_t size = value->value.array.size;
	
	void** array = malloc(sizeof(void*) * (size + 1));
	if (array == NULL)
		return NULL;
		
	for (size_t i = 0; i < size; i++) {
		array[i] = _json_unmarshall_value(type, &(value->value.array.entries[i]));
	}
	array[size] = NULL;
	
	return array;
}

void* _json_unmarshall_value(const char* type, jsonValue_t* value) {
	if (value->type == JSON_NULL) {
		return NULL;
	} else if (strcmp(type, "char") == 0) {
		return json_unmarshall_char(value);
	} else if (strcmp(type, "short") == 0) {
		return json_unmarshall_short(value);
	} else if (strcmp(type, "int") == 0) {
		return json_unmarshall_int(value);
	} else if (strcmp(type, "long") == 0) {
		return json_unmarshall_long(value);
	} else if (strcmp(type, "long long") == 0) {
		return json_unmarshall_long_long(value);
	} else if (strcmp(type, "float") == 0) {
		return json_unmarshall_float(value);
	} else if (strcmp(type, "double") == 0) {
		return json_unmarshall_double(value);
	} else if (strcmp(type, "string") == 0) {
		return json_unmarshall_string(value);
	} else if (strcmp(type, "bool") == 0) {
		return json_unmarshall_bool(value);	
	} else {
		struct marshaller* marshaller = findMarshaller(type);
		if (marshaller == NULL) {
			_marshallPanic(type, "unknown type");
		}
		return marshaller->unmarshaller(value);
	}
}

void* _json_unmarshall(const char* type, const char* json) {
	jsonValue_t* value = json_parse(json);
	if (value == NULL) {
		return NULL;
	}
	
	if (value->type == JSON_ARRAY) {
		json_free(value);
		return NULL;
	}
	
	void* tmp = _json_unmarshall_value(type, value);
	json_free(value);
	return tmp;
}

void* _json_unmarshall_array(const char* type, const char* json) {
	jsonValue_t* value = json_parse(json);
	if (value == NULL) {
		return NULL;
	}

	void* tmp = _json_unmarshall_array_value(type, value);
	json_free(value);
	return tmp;
}

void _json_free_struct(const char* type, void* value, bool this) {
	if (value == NULL) {
		return;
	} else if (strcmp(type, "char") == 0 ||
              strcmp(type, "short") == 0 ||
              strcmp(type, "int") == 0 ||
              strcmp(type, "long") == 0 ||
              strcmp(type, "long long") == 0 ||
              strcmp(type, "float") == 0 ||
              strcmp(type, "double") == 0 ||
              strcmp(type, "bool") == 0 ||
              strcmp(type, "string") == 0
	) {
		if (this)
			free(value);
	} else {
		struct marshaller* marshaller = findMarshaller(type);
		if (marshaller == NULL) {
			_marshallPanic(type, "unknown type");
		}
		marshaller->free(value, this);
	}
}

void _json_free_array(const char* type, void** value) {
	if (value == NULL) {
		return;
	} else if (strcmp(type, "char") == 0 ||
              strcmp(type, "short") == 0 ||
              strcmp(type, "int") == 0 ||
              strcmp(type, "long") == 0 ||
              strcmp(type, "long long") == 0 ||
              strcmp(type, "float") == 0 ||
              strcmp(type, "double") == 0 ||
              strcmp(type, "bool") == 0 ||
              strcmp(type, "string") == 0
	) {
		for (size_t i = 0; value[i] != NULL; i++) {
			free(value[i]);
		}
		free(value);
	} else {
		struct marshaller* marshaller = findMarshaller(type);
		if (marshaller == NULL) {
			_marshallPanic(type, "unknown type");
		}
		
		for (size_t i = 0; value[i] != NULL; i++) {
			marshaller->free(value[i], true);
		}
		
		free(value);
	}
}
