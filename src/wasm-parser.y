%{
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "wasm2.h"
%}

%define api.pure true
%define api.value.type {WasmToken}
%define api.token.prefix {WASM_TOKEN_TYPE_}
%define parse.error verbose
%lex-param {WasmScanner scanner}
%parse-param {WasmScanner scanner}
%locations

%token LPAR "("
%token RPAR ")"
%token INT FLOAT TEXT VAR VALUE_TYPE
%token NOP BLOCK IF IF_ELSE LOOP LABEL BR BR_IF TABLESWITCH CASE
%token CALL CALL_IMPORT CALL_INDIRECT RETURN
%token GET_LOCAL SET_LOCAL LOAD STORE LOAD_EXTEND STORE_WRAP OFFSET ALIGN
%token CONST UNARY BINARY COMPARE CONVERT CAST SELECT
%token FUNC TYPE PARAM RESULT LOCAL
%token MODULE MEMORY SEGMENT IMPORT EXPORT TABLE
%token UNREACHABLE MEMORY_SIZE GROW_MEMORY HAS_FEATURE
%token ASSERT_INVALID ASSERT_RETURN ASSERT_RETURN_NAN ASSERT_TRAP INVOKE
%token GLOBAL LOAD_GLOBAL STORE_GLOBAL PAGE_SIZE
%token EOF 0 "EOF"

%nonassoc LOW
%nonassoc VAR

%start script

%%

/* Types */

value_type_list :
    /* empty */
  | value_type_list VALUE_TYPE
;
func_type :
    /* empty */
  | LPAR PARAM value_type_list RPAR
  | LPAR PARAM value_type_list RPAR LPAR RESULT VALUE_TYPE RPAR
  | LPAR RESULT VALUE_TYPE RPAR
;


/* Expressions */

literal :
    INT
  | FLOAT
;

var :
    INT
  | VAR
;
var_list :
    /* empty */
  | var_list var
;
bind_var :
    VAR
;

labeling :
    /* empty */ %prec LOW
  | bind_var
;

offset :
    /* empty */
  | OFFSET
;
align :
    /* empty */
  | ALIGN
;

expr :
    LPAR expr1 RPAR
;
expr1 :
    NOP
  | BLOCK labeling expr expr_list
  | IF_ELSE expr expr expr
  | IF expr expr
  | BR_IF var expr
  | LOOP labeling labeling expr_list
  | LABEL labeling expr
  | BR expr_opt
  | BR var expr_opt
  | RETURN expr_opt
  | TABLESWITCH labeling expr LPAR TABLE target_list RPAR target case_list
  | CALL var expr_list
  | CALL_IMPORT var expr_list
  | CALL_INDIRECT var expr expr_list
  | GET_LOCAL var
  | SET_LOCAL var expr
  | LOAD offset align expr
  | STORE offset align expr expr
  | LOAD_EXTEND offset align expr
  | STORE_WRAP offset align expr expr
  | CONST literal
  | UNARY expr
  | BINARY expr expr
  | SELECT expr expr expr
  | COMPARE expr expr
  | CONVERT expr
  | CAST expr
  | UNREACHABLE
  | MEMORY_SIZE
  | GROW_MEMORY expr
  | HAS_FEATURE TEXT
  | LOAD_GLOBAL var
  | STORE_GLOBAL var expr
  | PAGE_SIZE
;
expr_opt :
    /* empty */
  | expr
;
expr_list :
    /* empty */
  | expr_list expr
;

target :
    LPAR CASE var RPAR
  | LPAR BR var RPAR
;
target_list :
    /* empty */
  | target_list target
;
case :
    LPAR CASE expr_list RPAR
  | LPAR CASE bind_var expr_list RPAR
;
case_list :
    /* empty */
  | case_list case
;


/* Functions */

func_fields :
    /* empty */
  | expr expr_list
  | LPAR PARAM value_type_list RPAR func_fields
  | LPAR PARAM bind_var VALUE_TYPE RPAR func_fields  /* Sugar */
  | LPAR RESULT VALUE_TYPE RPAR func_fields
  | LPAR LOCAL value_type_list RPAR func_fields
  | LPAR LOCAL bind_var VALUE_TYPE RPAR func_fields  /* Sugar */
;
type_use :
    LPAR TYPE var RPAR
;
func :
    LPAR FUNC type_use func_fields RPAR
  | LPAR FUNC bind_var type_use func_fields RPAR  /* Sugar */
  | LPAR FUNC func_fields RPAR  /* Sugar */
  | LPAR FUNC bind_var func_fields RPAR  /* Sugar */
;


/* Modules */

segment :
    LPAR SEGMENT INT TEXT RPAR
;
segment_list :
    /* empty */
  | segment_list segment
;

memory :
    LPAR MEMORY INT INT segment_list RPAR
  | LPAR MEMORY INT segment_list RPAR
;

type_def :
    LPAR TYPE LPAR FUNC func_type RPAR RPAR
  | LPAR TYPE bind_var LPAR FUNC func_type RPAR RPAR
;

table :
    LPAR TABLE var_list RPAR
;

import :
    LPAR IMPORT TEXT TEXT type_use RPAR
  | LPAR IMPORT bind_var TEXT TEXT type_use RPAR  /* Sugar */
  | LPAR IMPORT TEXT TEXT func_type RPAR  /* Sugar */
  | LPAR IMPORT bind_var TEXT TEXT func_type RPAR  /* Sugar */
;

export :
    LPAR EXPORT TEXT var RPAR
;

global :
    LPAR GLOBAL value_type_list RPAR
  | LPAR GLOBAL bind_var VALUE_TYPE RPAR
;

module_fields :
    /* empty */
  | func module_fields
  | import module_fields
  | export module_fields
  | table module_fields
  | type_def module_fields
  | memory module_fields
  | global module_fields
;
module :
    LPAR MODULE module_fields RPAR
;


/* Scripts */

cmd :
    module
  | LPAR INVOKE TEXT const_list RPAR
  | LPAR ASSERT_INVALID module TEXT RPAR
  | LPAR ASSERT_RETURN LPAR INVOKE TEXT const_list RPAR const_opt RPAR
  | LPAR ASSERT_RETURN_NAN LPAR INVOKE TEXT const_list RPAR RPAR
  | LPAR ASSERT_TRAP LPAR INVOKE TEXT const_list RPAR TEXT RPAR
;
cmd_list :
    /* empty */
  | cmd_list cmd
;

const :
    LPAR CONST literal RPAR
;
const_opt :
    /* empty */
  | const
;
const_list :
    /* empty */
  | const_list const
;

script :
    cmd_list EOF
;

%%

void yyerror(WasmLocation* loc, WasmScanner scanner, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "%s:%d:%d: ", loc->filename, loc->first_line,
          loc->first_column);
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
}
