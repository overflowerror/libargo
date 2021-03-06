#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <getopt.h>
#include <errno.h>

#include "codegen.h"
#include "helper.h"

struct declarsinfo* declarations;

extern FILE* yyin;
extern int yyparse();

void _panic(const char* f, const char* s) {
	fprintf(stderr, "panic: %s: %s: %s\n", f, s, strerror(errno));
	exit(3);
}


void generatePreamble(FILE* output, char* files[], int fileno) {
	fprintf(output, "#include <stdlib.h>\n");
	fprintf(output, "#include <stdbool.h>\n");
	fprintf(output, "#include <errno.h>\n");
	fprintf(output, "#include <alloca.h>\n");
	fprintf(output, "\n");
	fprintf(output, "#include <json.h>\n");
	fprintf(output, "#include <marshaller.h>\n");
	fprintf(output, "\n");
	
	for (int i = 0; i < fileno; i++) {
		char* base = basename(files[i]);
		fprintf(output, "#include <%s>\n", base);
	}
	
	fprintf(output, "\n");
	fprintf(output, "extern void _marshallPanic(const char*, const char*);\n");
	fprintf(output, "extern void _registerMarshaller(int, const char**, size_t, jsonValue_t*(*)(void*), void*(*)(jsonValue_t*), void(*)(void*, bool));\n");
	fprintf(output, "\n");
}

char* fixStructName(char* _name) {
	char* name = strdup(_name);
	if (name == NULL) {
		panic("strdup");
	}
	
	for (size_t i = 0; name[i] != '\0'; i++) {
		if (name[i] == ' ') {
			name[i] = '_';
		}
	}
	
	return name;
}

char* generateMarshallFunction(FILE* output, struct structinfo* info, char* suffix) {
	#define MARSHALL_FUNCTION_PREFIX "_json_marshall_value_"

	char* functionName = malloc(strlen(MARSHALL_FUNCTION_PREFIX) + strlen(suffix) + 1);
	if (functionName == NULL) {
		panic("malloc");
	}
	strcpy(functionName, MARSHALL_FUNCTION_PREFIX);
	strcat(functionName, suffix);
	
	fprintf(output, "static jsonValue_t* %s(void* _d) {\n", functionName);
	fprintf(output, "\t%s* d = (%s*) _d;\n", info->names[0], info->names[0]);
	fprintf(output, "\tif (d == NULL)\n");
	fprintf(output, "\t\treturn json_null();\n");
	fprintf(output, "\treturn json_object(true, %ld,\n", info->memberno);

	for (size_t i = 0; i < info->memberno; i++) {
		struct memberinfo* member = info->members[i];
		
		fprintf(output, "\t\t\"%s\", ", member->name);
		
		const char* comma = i == info->memberno - 1 ? "" : ",";
		
		if (member->type->isArray) {
			fprintf(output, "_json_marshall_array_value(\"%s\", (void*) (d->%s))%s\n", member->type->type, member->name, comma);
		} else {
			const char* reference = "";
			
			if (!member->type->isPointer && strcmp(member->type->type, "string") != 0) {
				reference = "&";
			}
			
			fprintf(output, "_json_marshall_value(\"%s\", (void*) %s(d->%s))%s\n", member->type->type, reference, member->name, comma);
		}
	}
	
	fprintf(output, "\t);\n");
	fprintf(output, "}\n\n");
	
	return functionName;
}

char* generateUnmarshallFunction(FILE* output, struct structinfo* info, char* suffix) {
	#define UNMARSHALL_FUNCTION_PREFIX "_json_unmarshall_value_"

	char* functionName = malloc(strlen(UNMARSHALL_FUNCTION_PREFIX) + strlen(suffix) + 1);
	if (functionName == NULL) {
		panic("malloc");
	}
	strcpy(functionName, UNMARSHALL_FUNCTION_PREFIX);
	strcat(functionName, suffix);
	
	fprintf(output, "static void* %s(jsonValue_t* v) {\n", functionName);
	fprintf(output, "\tif (v->type != JSON_OBJECT) {\n");
	fprintf(output, "\t\terrno = EINVAL;\n");
	fprintf(output, "\t\treturn NULL;\n");
	fprintf(output, "\t}\n");
	fprintf(output, "\t%s* d = malloc(sizeof(%s));\n", info->names[0], info->names[0]);
	fprintf(output, "\tif (d == NULL)\n");
	fprintf(output, "\t\treturn NULL;\n");
	fprintf(output, "\tvoid* tmp;\n");
	fprintf(output, "\tjsonValue_t* tmpValue;\n");
	
	for (size_t i = 0; i < info->memberno; i++) {
		struct memberinfo* member = info->members[i];
		fprintf(output, "\ttmpValue = json_object_get(v, \"%s\");\n", member->name);
		if (member->type->isArray) {
			fprintf(output, "\ttmp = _json_unmarshall_array_value(\"%s\", tmpValue);\n", member->type->type);
			fprintf(output, "\tjson_free(tmpValue);\n");
			const char* type = member->type->type;
			if (strcmp(type, "string") == 0) {
				type = "char";
			}
			fprintf(output, "\td->%s = (%s**) tmp;\n", member->name, type);
		} else {
			fprintf(output, "\ttmp = _json_unmarshall_value(\"%s\", tmpValue);\n", member->type->type);
			fprintf(output, "\tjson_free(tmpValue);\n");
			if (strcmp(member->type->type, "string") == 0) {
				fprintf(output, "\td->%s = (char*) tmp;\n", member->name);
			} else if (member->type->isPointer) {
				fprintf(output, "\td->%s = (%s*) tmp;\n", member->name, member->type->type);
			} else {
				fprintf(output, "\tif (tmp == NULL) {\n");
				fprintf(output, "\t\terrno = EINVAL;\n");
				fprintf(output, "\t\tfree(d);\n");
				fprintf(output, "\t\treturn NULL;\n");
				fprintf(output, "\t} else {\n");
				fprintf(output, "\t\td->%s = *((%s*) tmp);\n", member->name, member->type->type);
				fprintf(output, "\t\tfree(tmp);\n");
				fprintf(output, "\t}\n");
			}
		}
	}
	
	fprintf(output, "\treturn (void*) d;\n");
	
	fprintf(output, "}\n\n");
	
	return functionName;
}

char* generateFreeFunction(FILE* output, struct structinfo* info, char* suffix) {
	#define FREE_FUNCTION_PREFIX "_json_free_value_"

	char* functionName = malloc(strlen(FREE_FUNCTION_PREFIX) + strlen(suffix) + 1);
	if (functionName == NULL) {
		panic("malloc");
	}
	strcpy(functionName, FREE_FUNCTION_PREFIX);
	strcat(functionName, suffix);
	
	fprintf(output, "static void %s(void* _d, bool this) {\n", functionName);
	fprintf(output, "\tif (_d == NULL)\n");
	fprintf(output, "\t\treturn;\n");
	fprintf(output, "\t%s* d = (%s*) _d;\n", info->names[0], info->names[0]);
	for (size_t i = 0; i < info->memberno; i++) {
		struct memberinfo* member = info->members[i];
		if (member->type->isArray) {
			fprintf(output, "\t_json_free_array(\"%s\", (void**) d->%s);\n", member->type->type, member->name);
		} else if (member->type->isPointer || strcmp(member->type->type, "string") == 0) {
			fprintf(output, "\t_json_free_struct(\"%s\", (void*) d->%s, true);\n", member->type->type, member->name);
		} else if (!(strcmp(member->type->type, "char") == 0 ||
		             strcmp(member->type->type, "short") == 0 ||
		             strcmp(member->type->type, "int") == 0 ||
		             strcmp(member->type->type, "long") == 0 ||
		             strcmp(member->type->type, "long long") == 0 ||
		             strcmp(member->type->type, "float") == 0 ||
		             strcmp(member->type->type, "double") == 0 ||
		             strcmp(member->type->type, "bool") == 0
		)) {
			fprintf(output, "\t_json_free_struct(\"%s\", (void*) &(d->%s), false);\n", member->type->type, member->name);
		} else {
			// will be freed with this
		}
	}
	fprintf(output, "\tif (this)\n");
	fprintf(output, "\t\tfree(d);\n");
	fprintf(output, "}\n\n");
	
	return functionName;
}

void generateCodeStruct(FILE* output, struct structinfo* info) {
	fprintf(output, "// struct: %s\n\n", info->names[0]);

	char* suffix = fixStructName(info->names[0]);
	
	char* marshall = generateMarshallFunction(output, info, suffix);
	char* unmarshall = generateUnmarshallFunction(output, info, suffix);
	char* freeStruct = generateFreeFunction(output, info, suffix);
	
	
	fprintf(output, "__attribute__((constructor)) static void _register_marshaller_%s_() {\n", suffix);
	int namesno = info->names[1] == 0 ? 1 : 2;
	fprintf(output, "\tconst char** tmp = alloca(sizeof(char*) * %d);\n", namesno);
	fprintf(output, "\ttmp[0] = \"%s\";\n", info->names[0]);
	if (namesno > 1)
		fprintf(output, "\ttmp[1] = \"%s\";\n", info->names[1]);
	fprintf(output, "\t_registerMarshaller(%d, tmp, sizeof(%s), &%s, &%s, &%s);\n", namesno, info->names[0], marshall, unmarshall, freeStruct);
	fprintf(output, "}\n\n");
	
	free(suffix);
	
	free(marshall);
	free(unmarshall);
}

void generateCode(FILE* output, struct declarsinfo* declarations, const char* filename) {
	fprintf(output, "/*\n");
	fprintf(output, " * file: %s\n", filename);
	fprintf(output, "*/\n\n");

	for (size_t i = 0; i < declarations->structno; i++) {
		generateCodeStruct(output, declarations->structs[i]);
	}
}

#define MAX_FILES (10)

int main(int argc, char** argv) {
	FILE* output = stdout;
	char* files[MAX_FILES];
	FILE* input[MAX_FILES];
	struct declarsinfo* parsed[MAX_FILES];
	int fileno = 0;

	int opt;
	while((opt = getopt(argc, argv, "o:")) != -1) {
		switch(opt) {
			case 'o':
				output = fopen(optarg, "w+");
				if (output == NULL) {
					panic("fopen");
				}
				break;
			default:
				fprintf(stderr, "options: -o FILE\n");
				return 3;
		}
	}
	
	if (optind >= argc) {
		input[0] = stdin;
		fileno = 1;
	} else {
		while(optind < argc) {
			if (fileno + 1 >= MAX_FILES) {
				fprintf(stderr, "file limit reached\n");
				fprintf(stderr, "re-compile with larger limit\n");
				return 3;
			}
			
			files[fileno] = argv[optind];
			input[fileno] = fopen(argv[optind], "r");
			if (input[fileno] == NULL) {
				panic(argv[optind]);
			}
			
			fileno++;
			optind++;
		}
	}
	
	for (int i = 0; i < fileno; i++) {
		yyin = input[i];
	
		int result = yyparse(files[i]);
		if (result != 0) {
			return result;
		}
		
		parsed[i] = declarations;
	}
	
	generatePreamble(output, files, fileno);
	
	for (int i = 0; i < fileno; i++) {
		generateCode(output, parsed[i], files[i]);
	}
	
	return 0;
}
