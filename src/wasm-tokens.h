#ifndef WASM_TOKENS_H_
#define WASM_TOKENS_H_

typedef enum WasmType {
  WASM_TYPE_VOID = 0,
  WASM_TYPE_I32 = 1,
  WASM_TYPE_I64 = 2,
  WASM_TYPE_F32 = 4,
  WASM_TYPE_F64 = 8,
  WASM_TYPE_ALL = 15,
} WasmType;

typedef enum WasmMemSize {
  WASM_MEM_SIZE_8,
  WASM_MEM_SIZE_16,
  WASM_MEM_SIZE_32,
} WasmMemSize;

typedef enum WasmSigned {
  WASM_SIGNED,
  WASM_UNSIGNED,
} WasmSigned;

typedef enum WasmUnaryOp {
  WASM_UNARY_OP_CLZ,
  WASM_UNARY_OP_CTZ,
  WASM_UNARY_OP_POPCNT,
  WASM_UNARY_OP_NEG,
  WASM_UNARY_OP_ABS,
  WASM_UNARY_OP_SQRT,
  WASM_UNARY_OP_CEIL,
  WASM_UNARY_OP_FLOOR,
  WASM_UNARY_OP_TRUNC,
  WASM_UNARY_OP_NEAREST,
} WasmUnaryOp;

typedef enum WasmBinaryOp {
  WASM_BINARY_OP_ADD,
  WASM_BINARY_OP_SUB,
  WASM_BINARY_OP_MUL,
  WASM_BINARY_OP_DIV,
  WASM_BINARY_OP_REM,
  WASM_BINARY_OP_AND,
  WASM_BINARY_OP_OR,
  WASM_BINARY_OP_XOR,
  WASM_BINARY_OP_SHL,
  WASM_BINARY_OP_SHR,
  WASM_BINARY_OP_MIN,
  WASM_BINARY_OP_MAX,
  WASM_BINARY_OP_COPYSIGN,
} WasmBinaryOp;

typedef enum WasmCompareOp {
  WASM_COMPARE_OP_EQ,
  WASM_COMPARE_OP_NE,
  WASM_COMPARE_OP_LT,
  WASM_COMPARE_OP_LE,
  WASM_COMPARE_OP_GT,
  WASM_COMPARE_OP_GE,
} WasmCompareOp;

typedef struct WasmToken {
  const char* start;
  const char* end;
  WasmType type;
  union {
    struct {
      WasmUnaryOp op;
    } unary;
    struct {
      WasmBinaryOp op;
      WasmSigned sign;
    } binary;
    struct {
      WasmCompareOp op;
      WasmSigned sign;
    } compare;
    struct {
      WasmType type2;
      WasmSigned sign;
    } convert;
    struct {
      WasmType type2;
    } cast;
    struct {
      WasmMemSize size;
      WasmSigned sign;
    } mem;
  };
} WasmToken;

typedef struct WasmLocation {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} WasmLocation;

#endif /* WASM_TOKENS_H_ */
