/*
 * Copyright 2016 WebAssembly Community Group participants
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WASM_PARSER_LEXER_SHARED_H_
#define WASM_PARSER_LEXER_SHARED_H_

#include <stdarg.h>

#include "wasm-ast.h"
#include "wasm-common.h"
#include "wasm-lexer.h"

#define WASM_PARSER_STYPE WasmToken
#define WASM_PARSER_LTYPE WasmLocation
#define YYSTYPE WASM_PARSER_STYPE
#define YYLTYPE WASM_PARSER_LTYPE

#define WASM_INVALID_LINE_OFFSET ((size_t)~0)

struct WasmAllocator;

typedef union WasmToken {
  /* terminals */
  WasmStringSlice text;
  WasmType type;
  WasmOpcode opcode;
  WasmLiteral literal;

  /* non-terminals */
  /* some of these use pointers to keep the size of WasmToken down; copying the
   tokens is a hotspot when parsing large files. */
  uint32_t u32;
  uint64_t u64;
  WasmTypeVector types;
  WasmVar var;
  WasmVarVector vars;
  WasmExprPtr expr;
  WasmExprPtrVector exprs;
  WasmFuncField* func_fields;
  WasmFunc* func;
  WasmSegment segment;
  WasmSegmentVector segments;
  WasmMemory memory;
  WasmFuncSignature func_sig;
  WasmFuncType func_type;
  WasmImport* import;
  WasmExport export_;
  WasmExportMemory export_memory;
  WasmModule* module;
  WasmConst const_;
  WasmConstVector consts;
  WasmCommand* command;
  WasmCommandVector commands;
  WasmScript script;
} WasmToken;

typedef struct WasmParser {
  struct WasmAllocator* allocator;
  WasmScript script;
  WasmSourceErrorHandler* error_handler;
  int errors;
} WasmParser;

WASM_EXTERN_C_BEGIN
struct WasmAllocator* wasm_lexer_get_allocator(WasmLexer* lexer);
int wasm_lexer_lex(union WasmToken*,
                   struct WasmLocation*,
                   WasmLexer*,
                   struct WasmParser*);
WasmResult wasm_lexer_get_source_line(WasmLexer*,
                                      const struct WasmLocation*,
                                      size_t line_max_length,
                                      char* line,
                                      size_t* out_line_length,
                                      int* out_column_offset);
void WASM_PRINTF_FORMAT(4, 5) wasm_parser_error(struct WasmLocation*,
                                                WasmLexer*,
                                                struct WasmParser*,
                                                const char*,
                                                ...);
void wasm_format_error(WasmSourceErrorHandler*,
                       const struct WasmLocation*,
                       WasmLexer*,
                       const char* format,
                       va_list);
WASM_EXTERN_C_END

#endif /* WASM_PARSER_LEXER_SHARED_H_ */
