#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <gc.h>
#include <ffi.h>
#include <utf8proc.h>

#include "lasca.h"

#define STR(s) {.length = sizeof(s) - 1, .bytes = s}

// Primitive Types
const LaType _UNKNOWN = { .name = "Unknown" };
const LaType _UNIT    = { .name = "Unit" };
const LaType _BOOL    = { .name = "Bool" };
const LaType _BYTE    = { .name = "Byte" };
const LaType _INT     = { .name = "Int" };
const LaType _DOUBLE  = { .name = "Double" };
const LaType _STRING  = { .name = "String" };
const LaType _CLOSURE = { .name = "Closure" };
const LaType _ARRAY   = { .name = "Array" };
const LaType _REF     = { .name = "Ref" };
const LaType _BYTEARRAY     = { .name = "ByteArray" };
const LaType _FILE_HANDLE   = { .name = "FileHandle" };
const LaType _PATTERN = { .name = "Pattern" };
const LaType* UNKNOWN = &_UNKNOWN;
const LaType* UNIT    = &_UNIT;
const LaType* BOOL    = &_BOOL;
const LaType* BYTE    = &_BYTE;
const LaType* INT     = &_INT;
const LaType* DOUBLE  = &_DOUBLE;
const LaType* STRING  = &_STRING;
const LaType* CLOSURE = &_CLOSURE;
const LaType* ARRAY   = &_ARRAY;
const LaType* REF   = &_REF;
const LaType* BYTEARRAY   = &_BYTEARRAY;
const LaType* FILE_HANDLE   = &_FILE_HANDLE;
const LaType* PATTERN   = &_PATTERN;

Box TRUE_SINGLETON = {
    .type = &_BOOL,
    .value.num = 1
};
Box FALSE_SINGLETON = {
    .type = &_BOOL,
    .value.num = 0
};
Box UNIT_SINGLETON = {
    .type = &_UNIT
};
String EMPTY_STRING = STR("\00");
Box EMPTY_STRING_BOX = {
    .type = &_STRING,
    .value.ptr = &EMPTY_STRING
};
Box * UNIT_STRING;
Box  INT_ARRAY[100];
Box  DOUBLE_ZERO = {
    .type = &_DOUBLE,
    .value.dbl = 0.0
};
Environment ENV;
Runtime* RUNTIME;

bool eqTypes(const LaType* lhs, const LaType* rhs) {
    return lhs == rhs || strcmp(lhs->name, rhs->name) == 0;
}

void *gcMalloc(size_t s) {
    return GC_malloc(s);
}

void *gcMallocAtomic(size_t s) {
    return GC_malloc_atomic(s);
}

void *gcRealloc(void* old, size_t s) {
    return GC_realloc(old, s);
}


const char * __attribute__ ((const)) typeIdToName(const LaType* typeId) {
    return typeId->name;
}

/* =============== Boxing ================== */

Box *box(const LaType* type_id, void *value) {
    Box* ti = gcMalloc(sizeof(Box));
    ti->type = type_id;
    ti->value.ptr = value;
    return ti;
}

Box * __attribute__ ((pure)) boxBool(int64_t i) {
    switch (i) {
        case 0: return &FALSE_SINGLETON; break;
        default: return &TRUE_SINGLETON; break;
    }
}

Box * __attribute__ ((pure)) boxError(String *name) {
    return box(UNKNOWN, name);
}

Box * __attribute__ ((pure)) boxInt(int64_t i) {
    if (i >= 0 && i < 100) return &INT_ARRAY[i];
    else {
        Box* ti = gcMallocAtomic(sizeof(Box));
        ti->type = INT;
        ti->value.num = i;
        return ti;
    }
}

Box * __attribute__ ((pure)) boxFloat64(double i) {
    if (i == 0.0) return &DOUBLE_ZERO;
    Box* ti = gcMallocAtomic(sizeof(Box));
    ti->type = DOUBLE;
    ti->value.dbl = i;
    return ti;
}

Box * boxClosure(int64_t idx, int64_t argc, Box** args) {
    Closure* cl = gcMalloc(sizeof(Closure));
  //  printf("boxClosure(%d, %d, %p)\n", idx, argc, args);
  //  fflush(stdout);
    cl->funcIdx = idx;
    cl->argc = argc;
    cl->argv = args;
  //  printf("Enclose %d, argc = %d, args[0].type = %d, args[1].type = %d\n", idx, argc, args[0]->type, args[1]->type);
  //  fflush(stdout);
    return box(CLOSURE, cl);
}

void * __attribute__ ((pure)) unbox(const LaType* expected, const Box* ti) {
  //  printf("unbox(%d, %d) ", ti->type, (int64_t) ti->value);
    /* In most cases we can use pointer comparison,
       but when we use Lasca defined type in C code, we also define a LaType
       and we need to compare actual qualified type names.
       TODO/FIXME: think how to make it better, now it's O(typename_length), show be O(1)
       Likely, not an issue anyway.
    */
    if (eqTypes(ti->type, expected)) {
        return ti->value.ptr;
    } else if (eqTypes(ti->type, UNKNOWN)) {
        String *name = (String *) ti->value.ptr;
        printf("AAAA!!! Undefined identifier %s\n", name->bytes);
        exit(1);
    } else {
        printf("AAAA!!! Expected %s but got %s %p != %p\n", typeIdToName(expected), typeIdToName(ti->type), expected, ti->type);
        exit(1);
    }
}

int64_t __attribute__ ((pure)) unboxInt(const Box* ti) {
  //  printf("unbox(%d, %d) ", ti->type, (int64_t) ti->value);
    if (eqTypes(ti->type, INT)) {
        return ti->value.num;
    } else {
        printf("AAAA!!! Expected %s but got %s\n", typeIdToName(INT), typeIdToName(ti->type));
        exit(1);
    }
}

double __attribute__ ((pure)) unboxFloat64(Box* ti) {
  //  printf("unbox(%d, %d) ", ti->type, (int64_t) ti->value);
    if (eqTypes(ti->type, DOUBLE)) {
        return ti->value.dbl;
    } else {
        printf("AAAA!!! Expected %s but got %s\n", typeIdToName(DOUBLE), typeIdToName(ti->type));
        exit(1);
    }
}

/* ==================== Runtime Ops ============== */

Box* updateRef(Box* ref, Box* value) {
    assert(eqTypes(ref->type, REF));
    DataValue* dataValue = ref->value.ptr;
    Box* oldValue = dataValue->values[0];
    dataValue->values[0] = value;
    return oldValue;
}

static int64_t isBuiltinType(const Box* v) {
    const LaType* t = v->type;
    return eqTypes(t, UNIT) || eqTypes(t, BOOL) || eqTypes(t, BYTE) || eqTypes(t, INT) || eqTypes(t, DOUBLE)
      || eqTypes(t, STRING) || eqTypes(t, CLOSURE) || eqTypes(t, ARRAY) || eqTypes(t, BYTEARRAY);
}

static int64_t isUserType(const Box* v) {
    return !isBuiltinType(v);
}

#define DO_OP(op) if (eqTypes(lhs->type, INT)) { result = boxInt(lhs->value.num op rhs->value.num); } \
                  else if (eqTypes(lhs->type, BYTE)) { result = box(BYTE, (void*)(size_t)(lhs->value.byte op rhs->value.byte)); } \
                  else if (eqTypes(lhs->type, DOUBLE)) { result = boxFloat64(lhs->value.dbl op rhs->value.dbl); } \
                  else { \
                        printf("AAAA!!! Type mismatch! Expected Int or Double for op but got %s\n", typeIdToName(lhs->type)); exit(1); }

#define DO_CMP(op) if (eqTypes(lhs->type, BOOL)) { \
                      result = boxBool (lhs->value.num op rhs->value.num); } \
                   else if (eqTypes(lhs->type, INT)) { \
                      result = boxBool (lhs->value.num op rhs->value.num); } \
                   else if (eqTypes(lhs->type, BYTE)) { \
                      result = boxBool (lhs->value.byte op rhs->value.byte); } \
                   else if (eqTypes(lhs->type, DOUBLE)) { \
                      result = boxBool (lhs->value.dbl op rhs->value.dbl); } \
                   else { \
                      printf("AAAA!!! Type mismatch! Expected Bool, Int or Double but got %s\n", typeIdToName(lhs->type)); exit(1); \
                   }

Box* __attribute__ ((pure)) runtimeBinOp(int64_t code, Box* lhs, Box* rhs) {
    if (!eqTypes(lhs->type, rhs->type)) {
        printf("AAAA!!! Type mismatch! lhs = %s, rhs = %s\n", typeIdToName(lhs->type), typeIdToName(rhs->type));
        exit(1);
    }

    Box* result = NULL;

    if (code == ADD) { DO_OP(+); } 
    else if (code == SUB) { DO_OP(-); }
    else if (code == MUL) {DO_OP(*);}
    else if (code == DIV) {DO_OP(/);}
    else if (code == EQ) {DO_CMP(==);}
    else if (code == NE) {DO_CMP(!=);}
    else if (code == LT) {DO_CMP(<);}
    else if (code == LE) {DO_CMP(<=);}
    else if (code == GE) {DO_CMP(>=);}
    else if (code == GT) {DO_CMP(>);}
    else { 
	      printf("AAAA!!! Unsupported binary operation %"PRId64, code);
        exit(1);
    }
    return result;
}

Box* __attribute__ ((pure)) runtimeUnaryOp(int64_t code, Box* expr) {
    Box* result = NULL;
    switch (code) {
        case 1:
            if (eqTypes(expr->type, INT)) {
                result = boxInt(-expr->value.num);
            } else if (eqTypes(expr->type, DOUBLE)) {
                result = boxFloat64(-expr->value.dbl);
            } else {
                printf("AAAA!!! Type mismatch! Expected Int or Double for op but got %s\n", typeIdToName(expr->type));
                exit(1);
            }
            break;
        default:
            printf("AAAA!!! Unsupported unary operation %"PRId64, code);
            exit(1);
    }
    return result;
}

String UNIMPLEMENTED_SELECT = {
    .length = 20,
    .bytes = "Unimplemented select"
};

Box* runtimeApply(Box* val, int64_t argc, Box* argv[], Position pos) {
    Functions* fs = RUNTIME->functions;
    Closure *closure = unbox(CLOSURE, val);
    if (closure->funcIdx >= fs->size) {
        printf("AAAA!!! No such function with id %"PRId64", max id is %"PRId64" at line: %"PRId64"\n", (int64_t) closure->funcIdx, fs->size, pos.line);
        exit(1);
    }
    Function f = fs->functions[closure->funcIdx];
    if (f.arity != argc + closure->argc) {
        printf("AAAA!!! Function %s takes %"PRId64" params, but passed %"PRId64" enclosed params and %"PRId64" params instead at line: %"PRId64"\n",
            f.name->bytes, f.arity, closure->argc, argc, pos.line);
        exit(1);
    }

    ffi_cif cif;
    ffi_type *args[f.arity];
    void *values[f.arity];
    Box* rc;

    for (int i = 0; i < f.arity; i++) {
        args[i] = &ffi_type_pointer;
        values[i] = (i < closure->argc) ? &closure->argv[i] : &argv[i - closure->argc];
    }
    if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, f.arity, &ffi_type_pointer, args) != FFI_OK) {
    		printf("AAAA!!! Function %s ffi_prep_cif call failed\n", f.name->bytes);
        exit(1);
    }
    ffi_call(&cif, f.funcPtr, &rc, values);
    return rc;
}

Data* findDataType(const LaType* type) {
    Types* types = RUNTIME->types;
    for (int i = 0; i < types->size; i++) {
        if (eqTypes(types->data[i]->type, type)) return types->data[i];
    }
    printf("AAAA! Couldn't find type %s", type->name);
    exit(1);
}

Box* __attribute__ ((pure)) runtimeSelect(Box* tree, Box* ident, Position pos) {
    Functions* fs = RUNTIME->functions;

//    printf("isUserType %s %p %p\n", tree->type->name, tree->type, &_UNKNOWN);
    if (isUserType(tree)) {

        DataValue* dataValue = tree->value.ptr;
        // if rhs is not a local ident, nor a function, try to find this field in lhs data structure
        if (eqTypes(ident->type, UNKNOWN)) {
            String* name = ident->value.ptr; // should be identifier name
//            printf("Ident name %s\n", name->bytes);
            Data* data = findDataType(tree->type); // find struct in global array of structs
//            printf("Found data type %s %s, tag %"PRId64"\n", data->name->bytes, tree->type->name, dataValue->tag);
            Struct* constr = data->constructors[dataValue->tag];
            int64_t numFields = constr->numFields;
            for (int64_t i = 0; i < numFields; i++) {
                String* field = constr->fields[i];
        //        printf("Check field %d %s\n", field->length, field->bytes);
                if (field->length == name->length && strncmp(field->bytes, name->bytes, name->length) == 0) {
                    Box* value = dataValue->values[i];
//                    printf("Found value %s at index %"PRId64"\n", value->type->name, i);
          //          println(toString(value));
                    return value;
                }
            }
            printf("Couldn't find field %s at line: %"PRId64"\n", name->bytes, pos.line);
        } else if (eqTypes(ident->type, CLOSURE)) {
              // FIXME fix for closure?  check arity?
              Closure* f = unbox(CLOSURE, ident);
              assert(fs->functions[f->funcIdx].arity == 1);
              return runtimeApply(ident, 1, &tree, pos);
        }
    } else if (eqTypes(ident->type, CLOSURE)) {
        // FIXME fix for closure?  check arity?
        Closure* f = unbox(CLOSURE, ident);
        assert(fs->functions[f->funcIdx].arity == 1);
        return runtimeApply(ident, 1, &tree, pos);
    }
    return boxError(&UNIMPLEMENTED_SELECT);
}

int64_t runtimeIsConstr(Box* value, Box* constrName) {
    if (isUserType(value)) {
        String* name = unbox(STRING, constrName);
        Data* data = findDataType(value->type);
        DataValue* dv = value->value.ptr;
        String* realConstrName = data->constructors[dv->tag]->name;
        if (strncmp(realConstrName->bytes, name->bytes, fmin(realConstrName->length, name->length)) == 0)
            return true;
    }
    return false;
}

int64_t runtimeCheckTag(Box* value, int64_t tag) {
    DataValue* dv = value->value.ptr;
    return dv->tag == tag;
}

/* =================== Arrays ================= */


Array* createArray(size_t size) {
    Array * array = gcMalloc(sizeof(Array) + sizeof(Box*) * size);
    array->length = size;
    return array;
}

Box* boxArray(size_t size, ...) {
    va_list argp;
    Array * array = createArray(size);
    va_start (argp, size);
    for (int64_t i = 0; i < size; i++) {
        Box* arg = va_arg(argp, Box*);
        array->data[i] = arg;
    }
    va_end (argp);                  /* Clean up. */
    return box(ARRAY, array);
}

Box* __attribute__ ((pure)) makeString(char * str) {
    size_t len = strlen(str);
    String* val = gcMalloc(sizeof(String) + len + 1);  // null terminated
    val->length = len;
    strncpy(val->bytes, str, len);
    return box(STRING, val);
}

Box* joinValues(int size, Box* values[], char* start, char* end) {
    String* strings[size];
    int startLen = strlen(start);
    int endLen = strlen(end);
    int resultSize = startLen + endLen + 1 + 2 * size;

    for (int i = 0; i < size; i++) {
        Box* elem = values[i];
        String* value = unbox(STRING, toString(elem));
        resultSize += value->length;
        strings[i] = value;
    }

    char * result = malloc(resultSize);
    strcpy(result, start);
    for (int i = 0; i < size; i++) {
        String* value = strings[i];
        strcat(result, value->bytes);
        if (i + 1 < size) strcat(result, ", ");
    }
    strcat(result, end);
    Box* string = makeString(result);
    free(result);
    return string;
}

Box* __attribute__ ((pure)) arrayToString(const Box* arrayValue)  {
    Array* array = unbox(ARRAY, arrayValue);
    if (array->length == 0) {
        return makeString("[]");
    } else {
        return joinValues(array->length, array->data, "[", "]");
    }
}

Box* __attribute__ ((pure)) byteArrayToString(const Box* arrayValue)  {
    String* array = unbox(BYTEARRAY, arrayValue);
    if (array->length == 0) {
        return makeString("[]");
    } else {
        int len = 6 * array->length + 2 + 1; // max (4 + 2 (separator)) symbols per byte + [] + 0
        String* res = gcMalloc(sizeof(String) + len);
        strcpy(res->bytes, "[");
        char buf[7];
        int curPos = 1;
        for (int i = 0; i < array->length; i++) {
            int l = (i < array->length - 1) ?
              snprintf(buf, 7, "%"PRId8", ", array->bytes[i]) : snprintf(buf, 5, "%"PRId8, array->bytes[i]);
            strcpy(res->bytes + curPos, buf);
            curPos += l;
        }
        strcpy(res->bytes + curPos, "]");
        res->length = curPos + 1;
        return box(STRING, res);
    }
}


/* =============== Strings ============= */

const Box* __attribute__ ((pure)) toString(const Box* value) {
    char buf[100]; // 100 chars is enough for all (c)

    const LaType* type = value->type;
    if (eqTypes(type, UNIT)) {
        return UNIT_STRING;
    } else if (eqTypes(type, BOOL)) {
        return makeString(value->value.num == 0 ? "false" : "true");
    } else if (eqTypes(type, INT)) {
        snprintf(buf, 100, "%"PRId64, value->value.num);
        return makeString(buf);}
    else if (eqTypes(type, BYTE)) {
        snprintf(buf, 100, "%"PRId8, value->value.byte);
        return makeString(buf);
    } else if (eqTypes(type, DOUBLE)) {
        snprintf(buf, 100, "%12.9lf", value->value.dbl);
        return makeString(buf);
    } else if (eqTypes(type, STRING)) {
        return value;
    } else if (eqTypes(type, CLOSURE)) {
        return makeString("<func>");
    } else if (eqTypes(type, ARRAY)) {
        return arrayToString(value);
    } else if (eqTypes(type, BYTEARRAY)) {
        return byteArrayToString(value);
    } else if (eqTypes(type, REF)) {
        DataValue* dataValue = value->value.ptr;
        return toString(dataValue->values[0]);
    } else if (eqTypes(type, UNKNOWN)) {
        String *name = (String *) value->value.ptr;
        printf("AAAA!!! Undefined identifier in toString %s\n", name->bytes);
        exit(1);
    } else {
        if (isUserType(value)) {
            DataValue* dataValue = value->value.ptr;
            Data* metaData = findDataType(type);
            Struct* constr = metaData->constructors[dataValue->tag];
            int64_t startlen = constr->name->length + 2; // ending 0 and possibly "(" if constructor has parameters
            char start[startlen];
            snprintf(start, startlen, "%s", constr->name->bytes);
            if (constr->numFields > 0) {
                strcat(start, "(");
                return joinValues(constr->numFields, dataValue->values, start, ")");
            } else return makeString(start);
        } else {
            printf("Unsupported type %s", typeIdToName(value->type));
            exit(1);
        }
    }
}

Box* concat(Box* arrayString) {
    Array* array = unbox(ARRAY, arrayString);
    Box* result = &EMPTY_STRING_BOX;
    if (array->length > 0) {
        int64_t len = 0;
        for (int64_t i = 0; i < array->length; i++) {
            String* s = unbox(STRING, array->data[i]);
            len += s->length;
        }
        String* val = gcMalloc(sizeof(String) + len + 1); // +1 for null-termination
        // val->length is 0, because gcMalloc allocates zero-initialized memory
        // it's also zero terminated, because gcMalloc allocates zero-initialized memory
        for (int64_t i = 0; i < array->length; i++) {
            String* s = unbox(STRING, array->data[i]);
            memcpy(&val->bytes[val->length], s->bytes, s->length);
            val->length += s->length;
        }
        result = box(STRING, val);
    }
    return result;
}

/* ============ System ================ */

void initEnvironment(int64_t argc, char* argv[]) {
  //  int64_t len = 0;
  //  for (int64_t i = 0; i< argc; i++) len += strlen(argv[i]);
  //  char buf[len + argc*2 + 10];
  //  for (int64_t i = 0; i < argc; i++) {
  //    strcat(buf, argv[i]);
  //    strcat(buf, " ");
  //  }
  //  printf("Called with %d \n", argc);
    ENV.argc = argc;
    Array* array = createArray(argc);
    for (int64_t i = 0; i < argc; i++) {
        Box* s = makeString(argv[i]);
        array->data[i] = s;
    }
    ENV.argv = box(ARRAY, array);
}

Box* getArgs() {
    return ENV.argv;
}

int8_t intToByte(int64_t n) {
    return (int8_t) n;
}

int64_t byteToInt(int8_t n) {
    return (int64_t) n;
}

int64_t toInt(Box* s) {
    String* str = unbox(STRING, s);
  //  println(s);
    char cstr[str->length + 1];
    memcpy(cstr, str->bytes, str->length); // TODO use VLA?
    cstr[str->length] = 0;
  //  printf("cstr = %s\n", cstr);
    char *ep;
    long i = strtol(cstr, &ep, 10);
    if (cstr == ep) {
        printf("Couldn't convert %s to int64_t", cstr);
        exit( EXIT_FAILURE );
    }
    return (int64_t) i;
}

void initLascaRuntime(Runtime* runtime) {
    GC_init();
    GC_expand_hp(4*1024*1024);
    RUNTIME = runtime;
    UNIT_STRING = makeString("()");
    for (int i = 0; i < 100; i++) {
        INT_ARRAY[i].type = INT;
        INT_ARRAY[i].value.num = i;
    }
    if (runtime->verbose)
        printf("Init Lasca 0.0.0.1 runtime. Enjoy :)\n# funcs = %"PRId64
               ", # structs = %"PRId64", utf8proc version %s\n",
          RUNTIME->functions->size, RUNTIME->types->size, utf8proc_version());
}
