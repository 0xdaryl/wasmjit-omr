#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FUNCTION_ARGS 10
#define TABS_TO_SPACES 8
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define FATAL(...) fprintf(stderr, __VA_ARGS__), exit(1)

typedef enum Type {
  TYPE_VOID,
  TYPE_I32,
  TYPE_I64,
  TYPE_F32,
  TYPE_F64,
} Type;

typedef enum TokenType {
  TOKEN_TYPE_EOF,
  TOKEN_TYPE_OPEN_PAREN,
  TOKEN_TYPE_CLOSE_PAREN,
  TOKEN_TYPE_ATOM,
  TOKEN_TYPE_STRING,
} TokenType;

typedef struct SourceLocation {
  const char* pos;
  int line;
  int col;
} SourceLocation;

typedef struct SourceRange {
  SourceLocation start;
  SourceLocation end;
} SourceRange;

typedef struct Token {
  TokenType type;
  SourceRange range;
} Token;

typedef struct Source {
  const char* start;
  const char* end;
} Source;

typedef struct Tokenizer {
  Source source;
  SourceLocation loc;
} Tokenizer;

typedef struct Function {
  const char* name;
  Type result_type;
  int num_args;
  Type arg_types[MAX_FUNCTION_ARGS];
} Function;

typedef struct Global {
  const char* name;
  Type type;
} Global;

typedef struct Export {
  const char* name;
  int index;
} Export;

typedef struct Module {
  Function* functions;
  Global* globals;
  Export* exports;
  int num_functions;
  int num_globals;
  int num_exports;
} Module;

typedef struct NameTypePair {
  const char* name;
  Type type;
} NameTypePair;

typedef struct NameType2Pair {
  const char* name;
  Type in_type;
  Type out_type;
} NameType2Pair;

static NameTypePair s_unary_ops[] = {
    {"neg.i32", TYPE_I32},   {"neg.i64", TYPE_I64},   {"neg.f32", TYPE_F32},
    {"neg.f64", TYPE_F64},   {"abs.i32", TYPE_I32},   {"abs.i64", TYPE_I64},
    {"abs.f32", TYPE_F32},   {"abs.f64", TYPE_F64},   {"not.i32", TYPE_I32},
    {"not.i64", TYPE_I64},   {"not.f32", TYPE_F32},   {"not.f64", TYPE_F64},
    {"clz.i32", TYPE_I32},   {"clz.i64", TYPE_I64},   {"ctz.i32", TYPE_I32},
    {"ctz.i64", TYPE_I64},   {"ceil.f32", TYPE_F32},  {"ceil.f64", TYPE_F64},
    {"floor.f32", TYPE_F32}, {"floor.f64", TYPE_F64}, {"trunc.f32", TYPE_F32},
    {"trunc.f64", TYPE_F64}, {"round.f32", TYPE_F32}, {"round.f64", TYPE_F64},
};

static NameTypePair s_binary_ops[] = {
    {"add.i32", TYPE_I32},      {"add.i64", TYPE_I64},
    {"add.f32", TYPE_F32},      {"add.f64", TYPE_F64},
    {"sub.i32", TYPE_I32},      {"sub.i64", TYPE_I64},
    {"sub.f32", TYPE_F32},      {"sub.f64", TYPE_F64},
    {"mul.i32", TYPE_I32},      {"mul.i64", TYPE_I64},
    {"mul.f32", TYPE_F32},      {"mul.f64", TYPE_F64},
    {"divs.i32", TYPE_I32},     {"divs.i64", TYPE_I64},
    {"divu.i32", TYPE_I32},     {"divu.i64", TYPE_I64},
    {"div.f32", TYPE_F32},      {"div.f64", TYPE_F64},
    {"mods.i32", TYPE_I32},     {"mods.i64", TYPE_I64},
    {"modu.i32", TYPE_I32},     {"modu.i64", TYPE_I64},
    {"and.i32", TYPE_I32},      {"and.i64", TYPE_I64},
    {"or.i32", TYPE_I32},       {"or.i64", TYPE_I64},
    {"xor.i32", TYPE_I32},      {"xor.i64", TYPE_I64},
    {"shl.i32", TYPE_I32},      {"shl.i64", TYPE_I64},
    {"shr.i32", TYPE_I32},      {"shr.i64", TYPE_I64},
    {"sar.i32", TYPE_I32},      {"sar.i64", TYPE_I64},
    {"copysign.f32", TYPE_F32}, {"copysign.f64", TYPE_F64},
};

static NameType2Pair s_compare_ops[] = {
    {"eq.i32", TYPE_I32, TYPE_I32},  {"eq.i64", TYPE_I64, TYPE_I32},
    {"eq.f32", TYPE_F32, TYPE_I32},  {"eq.f64", TYPE_F64, TYPE_I32},
    {"neq.i32", TYPE_I32, TYPE_I32}, {"neq.i64", TYPE_I64, TYPE_I32},
    {"neq.f32", TYPE_F32, TYPE_I32}, {"neq.f64", TYPE_F64, TYPE_I32},
    {"lts.i32", TYPE_I32, TYPE_I32}, {"lts.i64", TYPE_I64, TYPE_I32},
    {"ltu.i32", TYPE_I32, TYPE_I32}, {"ltu.i64", TYPE_I64, TYPE_I32},
    {"lt.f32", TYPE_F32, TYPE_I32},  {"lt.f64", TYPE_F64, TYPE_I32},
    {"les.i32", TYPE_I32, TYPE_I32}, {"les.i64", TYPE_I64, TYPE_I32},
    {"leu.i32", TYPE_I32, TYPE_I32}, {"leu.i64", TYPE_I64, TYPE_I32},
    {"le.f32", TYPE_F32, TYPE_I32},  {"le.f64", TYPE_F64, TYPE_I32},
    {"gts.i32", TYPE_I32, TYPE_I32}, {"gts.i64", TYPE_I64, TYPE_I32},
    {"gtu.i32", TYPE_I32, TYPE_I32}, {"gtu.i64", TYPE_I64, TYPE_I32},
    {"gt.f32", TYPE_F32, TYPE_I32},  {"gt.f64", TYPE_F64, TYPE_I32},
    {"ges.i32", TYPE_I32, TYPE_I32}, {"ges.i64", TYPE_I64, TYPE_I32},
    {"geu.i32", TYPE_I32, TYPE_I32}, {"geu.i64", TYPE_I64, TYPE_I32},
    {"ge.f32", TYPE_F32, TYPE_I32},  {"ge.f64", TYPE_F64, TYPE_I32},
};

static NameType2Pair s_convert_ops[] = {
    {"converts.i32.i32", TYPE_I32, TYPE_I32},
    {"convertu.i32.i32", TYPE_I32, TYPE_I32},
    {"converts.i32.i64", TYPE_I32, TYPE_I64},
    {"convertu.i32.i64", TYPE_I32, TYPE_I64},
    {"converts.i64.i32", TYPE_I64, TYPE_I32},
    {"convertu.i64.i32", TYPE_I64, TYPE_I32},
    {"converts.i64.i64", TYPE_I64, TYPE_I64},
    {"convertu.i64.i64", TYPE_I64, TYPE_I64},
    {"converts.i32.f32", TYPE_I32, TYPE_F32},
    {"convertu.i32.f32", TYPE_I32, TYPE_F32},
    {"converts.i32.f64", TYPE_I32, TYPE_F64},
    {"convertu.i32.f64", TYPE_I32, TYPE_F64},
    {"converts.i64.f32", TYPE_I64, TYPE_F32},
    {"convertu.i64.f32", TYPE_I64, TYPE_F32},
    {"converts.i64.f64", TYPE_I64, TYPE_F64},
    {"convertu.i64.f64", TYPE_I64, TYPE_F64},
    {"converts.f32.i32", TYPE_F32, TYPE_I32},
    {"convertu.f32.i32", TYPE_F32, TYPE_I32},
    {"converts.f32.i64", TYPE_F32, TYPE_I64},
    {"convertu.f32.i64", TYPE_F32, TYPE_I64},
    {"converts.f64.i32", TYPE_F64, TYPE_I32},
    {"convertu.f64.i32", TYPE_F64, TYPE_I32},
    {"converts.f64.i64", TYPE_F64, TYPE_I64},
    {"convertu.f64.i64", TYPE_F64, TYPE_I64},
    {"convert.f32.f32", TYPE_F32, TYPE_F32},
    {"convert.f32.f64", TYPE_F32, TYPE_F64},
    {"convert.f64.f32", TYPE_F64, TYPE_F32},
    {"convert.f64.f64", TYPE_F64, TYPE_F64},
};

static NameType2Pair s_cast_ops[] = {
    {"cast.i32.f32", TYPE_I32, TYPE_F32},
    {"cast.f32.i32", TYPE_F32, TYPE_I32},
    {"cast.i64.f64", TYPE_I64, TYPE_F64},
    {"cast.f64.i64", TYPE_F64, TYPE_I64},
};

static NameTypePair s_const_ops[] = {
    {"const.i32", TYPE_I32},
    {"const.i64", TYPE_I64},
    {"const.f32", TYPE_F32},
    {"const.f64", TYPE_F64},
};

static NameTypePair s_types[] = {
    {"i32", TYPE_I32},
    {"i64", TYPE_I64},
    {"f32", TYPE_F32},
    {"f64", TYPE_F64},
};

static const char* s_mem_int_types[] = {
    "i8", "i16", "i32", "i64",
};

static const char* s_mem_float_types[] = {
    "f32", "f64",
};

static void realloc_list(void** elts, int* num_elts, int elt_size) {
  (*num_elts)++;
  int new_size = *num_elts * elt_size;
  *elts = realloc(*elts, new_size);
  if (*elts == NULL) {
    FATAL("unable to alloc %d bytes", new_size);
  }
}

static int get_global_by_name(Module* module, const char* name) {
  int i;
  for (i = 0; i < module->num_globals; ++i) {
    Global* global = &module->globals[i];
    if (global->name && strcmp(name, global->name) == 0)
      return i;
  }
  return -1;
}

static int read_uint32(const char** s, const char* end, uint32_t* out) {
  errno = 0;
  char* endptr;
  uint64_t value = strtoul(*s, &endptr, 10);
  if (endptr != end || errno != 0 || value >= (uint64_t)UINT32_MAX + 1)
    return 0;
  *out = value;
  *s = endptr;
  return 1;
}

static int read_uint64(const char** s, const char* end, uint64_t* out) {
  errno = 0;
  char* endptr;
  *out = strtoull(*s, &endptr, 10);
  if (endptr != end || errno != 0)
    return 0;
  *s = endptr;
  return 1;
}

static int read_double(const char** s, const char* end, double* out) {
  errno = 0;
  char* endptr;
  *out = strtod(*s, &endptr);
  if (endptr != end || errno != 0)
    return 0;
  *s = endptr;
  return 1;
}

static Token read_token(Tokenizer* t) {
  Token result;

  while (t->loc.pos < t->source.end) {
    switch (*t->loc.pos) {
      case ' ':
        t->loc.col++;
        t->loc.pos++;
        break;

      case '\t':
        t->loc.col += TABS_TO_SPACES;
        t->loc.pos++;
        break;

      case '\n':
        t->loc.line++;
        t->loc.col = 1;
        t->loc.pos++;
        break;

      case '"':
        result.type = TOKEN_TYPE_STRING;
        result.range.start = t->loc;
        t->loc.col++;
        t->loc.pos++;

        while (t->loc.pos < t->source.end) {
          switch (*t->loc.pos) {
            case '\\':
              if (t->loc.pos + 1 < t->source.end) {
                t->loc.col++;
                t->loc.pos++;
              }
              break;

            case '\n':
              FATAL("newline in string\n");
              break;

            case '"':
              t->loc.col++;
              t->loc.pos++;
              goto done_string;
          }

          t->loc.col++;
          t->loc.pos++;
        }
      done_string:
        result.range.end = t->loc;
        return result;

      case ';':
        if (t->loc.pos + 1 < t->source.end && t->loc.pos[1] == ';') {
          /* line comment */
          while (t->loc.pos < t->source.end) {
            if (*t->loc.pos == '\n')
              break;

            t->loc.col++;
            t->loc.pos++;
          }
        }
        break;

      case '(':
        if (t->loc.pos + 1 < t->source.end && t->loc.pos[1] == ';') {
          int nesting = 1;
          t->loc.pos += 2;
          t->loc.col += 2;
          while (nesting > 0 && t->loc.pos < t->source.end) {
            switch (*t->loc.pos) {
              case ';':
                if (t->loc.pos + 1 < t->source.end && t->loc.pos[1] == ')') {
                  nesting--;
                  t->loc.col++;
                  t->loc.pos++;
                }
                t->loc.col++;
                t->loc.pos++;
                break;

              case '(':
                if (t->loc.pos + 1 < t->source.end && t->loc.pos[1] == ';') {
                  nesting++;
                  t->loc.col++;
                  t->loc.pos++;
                }
                t->loc.col++;
                t->loc.pos++;
                break;

              case '\t':
                t->loc.col += TABS_TO_SPACES;
                t->loc.pos++;
                break;

              case '\n':
                t->loc.line++;
                t->loc.col = 1;
                t->loc.pos++;
                break;

              default:
                t->loc.pos++;
                t->loc.col++;
                break;
            }
          }
          break;
        } else {
          result.type = TOKEN_TYPE_OPEN_PAREN;
          result.range.start = t->loc;
          t->loc.col++;
          t->loc.pos++;
          result.range.end = t->loc;
          return result;
        }
        break;

      case ')':
        result.type = TOKEN_TYPE_CLOSE_PAREN;
        result.range.start = t->loc;
        t->loc.col++;
        t->loc.pos++;
        result.range.end = t->loc;
        return result;

      default:
        result.type = TOKEN_TYPE_ATOM;
        result.range.start = t->loc;
        t->loc.col++;
        t->loc.pos++;
        while (t->loc.pos < t->source.end) {
          switch (*t->loc.pos) {
            case ' ':
            case '\t':
            case '\n':
            case '(':
            case ')':
              result.range.end = t->loc;
              return result;

            default:
              t->loc.col++;
              t->loc.pos++;
              break;
          }
        }
        break;
    }
  }

  result.type = TOKEN_TYPE_EOF;
  result.range.start = t->loc;
  result.range.end = t->loc;
  return result;
}

static void rewind_token(Tokenizer* tokenizer, Token t) {
  tokenizer->loc = t.range.start;
}

#if 0
static void print_tokens(Tokenizer* tokenizer) {
  while (1) {
    Token token = read_token(&tokenizer);
    fprintf(stderr, "[%d:%d]:[%d:%d]: ", token.range.start.line,
            token.range.start.col, token.range.end.line, token.range.end.col);
    switch (token.type) {
      case TOKEN_TYPE_EOF:
        fprintf(stderr, "EOF\n");
        return;

      case TOKEN_TYPE_OPEN_PAREN:
        fprintf(stderr, "OPEN_PAREN\n");
        break;

      case TOKEN_TYPE_CLOSE_PAREN:
        fprintf(stderr, "CLOSE_PAREN\n");
        break;

      case TOKEN_TYPE_ATOM:
        fprintf(stderr, "ATOM: %.*s\n",
                (int)(token.range.end.pos - token.range.start.pos),
                token.range.start.pos);
        break;

      case TOKEN_TYPE_STRING:
        fprintf(stderr, "STRING: %.*s\n",
                (int)(token.range.end.pos - token.range.start.pos),
                token.range.start.pos);
        break;
    }
  }
}
#endif

static void expect_open(Token t) {
  if (t.type != TOKEN_TYPE_OPEN_PAREN) {
    FATAL("%d:%d: expected '(', not \"%.*s\"\n", t.range.start.line,
          t.range.start.col, (int)(t.range.end.pos - t.range.start.pos),
          t.range.start.pos);
  }
}

static void expect_close(Token t) {
  if (t.type != TOKEN_TYPE_CLOSE_PAREN) {
    FATAL("%d:%d: expected ')', not \"%.*s\"\n", t.range.start.line,
          t.range.start.col, (int)(t.range.end.pos - t.range.start.pos),
          t.range.start.pos);
  }
}

static void expect_atom(Token t) {
  if (t.type != TOKEN_TYPE_ATOM) {
    FATAL("%d:%d: expected ATOM, not \"%.*s\"\n", t.range.start.line,
          t.range.start.col, (int)(t.range.end.pos - t.range.start.pos),
          t.range.start.pos);
  }
}

static void expect_var_name(Token t) {
  if (t.type != TOKEN_TYPE_ATOM) {
    FATAL("%d:%d: expected ATOM, not \"%.*s\"\n", t.range.start.line,
          t.range.start.col, (int)(t.range.end.pos - t.range.start.pos),
          t.range.start.pos);
  }

  if (t.range.end.pos - t.range.start.pos < 1 ||
      t.range.start.pos[0] != '$') {
    FATAL("%d:%d: expected name to begin with $\n", t.range.start.line,
          t.range.start.col);
  }
}

static int match_atom(Token t, const char* s) {
  return strncmp(t.range.start.pos, s, t.range.end.pos - t.range.start.pos) ==
         0;
}

static int match_atom_prefix(Token t, const char* s) {
  size_t slen = strlen(s);
  size_t len = t.range.end.pos - t.range.start.pos;
  if (slen < len)
    len = slen;
  return strncmp(t.range.start.pos, s, slen) == 0;
}

static int match_unary(Token t, Type* type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_unary_ops); ++i) {
    if (match_atom(t, s_unary_ops[i].name)) {
      *type = s_unary_ops[i].type;
      return 1;
    }
  }
  return 0;
}

static int match_binary(Token t, Type* type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_binary_ops); ++i) {
    if (match_atom(t, s_binary_ops[i].name)) {
      *type = s_binary_ops[i].type;
      return 1;
    }
  }
  return 0;
}

static int match_compare(Token t, Type* in_type, Type* out_type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_compare_ops); ++i) {
    if (match_atom(t, s_compare_ops[i].name)) {
      *in_type = s_compare_ops[i].in_type;
      *out_type = s_compare_ops[i].out_type;
      return 1;
    }
  }
  return 0;
}

static int match_convert(Token t, Type* in_type, Type* out_type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_convert_ops); ++i) {
    if (match_atom(t, s_convert_ops[i].name)) {
      *in_type = s_convert_ops[i].in_type;
      *out_type = s_convert_ops[i].out_type;
      return 1;
    }
  }
  return 0;
}

static int match_cast(Token t, Type* in_type, Type* out_type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_cast_ops); ++i) {
    if (match_atom(t, s_cast_ops[i].name)) {
      *in_type = s_cast_ops[i].in_type;
      *out_type = s_cast_ops[i].out_type;
      return 1;
    }
  }
  return 0;
}

static int match_const(Token t, Type* type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_const_ops); ++i) {
    if (match_atom(t, s_const_ops[i].name)) {
      *type = s_const_ops[i].type;
      return 1;
    }
  }
  return 0;
}

static int match_type(Token t, Type* type) {
  int i;
  for (i = 0; i < ARRAY_SIZE(s_types); ++i) {
    if (match_atom(t, s_types[i].name)) {
      if (type) {
        *type = s_types[i].type;
      }
      return 1;
    }
  }
  return 0;
}

static int match_load_store(Token t, const char* prefix) {
  size_t plen = strlen(prefix);
  const char* p = t.range.start.pos;
  const char* end = t.range.end.pos;
  size_t len = end - p;
  const char** types = NULL;
  int num_types = 0;

  if (len < plen + 1)
    return 0;
  if (strncmp(p, prefix, plen) != 0)
    return 0;
  p += plen;
  switch (*p) {
    case 's':
    case 'u':
      types = s_mem_int_types;
      num_types = ARRAY_SIZE(s_mem_int_types);
      p++;
      if (p >= end || *p != '.')
        return 0;
      p++;
      break;

    case '.':
      types = s_mem_float_types;
      num_types = ARRAY_SIZE(s_mem_float_types);
      p++;
      break;

    default:
      return 0;
  }

  /* try to read alignment */
  if (p >= end)
    return 0;

  switch (*p) {
    case 'i':
    case 'f':
      break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
      /* find first non-digit */
      const char* align_end = p;
      while (align_end < end && *align_end >= '0' && *align_end <= '9')
        align_end++;

      uint32_t value;
      if (!read_uint32(&p, align_end, &value)) {
        FATAL("%d:%d: invalid alignment\n", t.range.start.line,
              t.range.start.col);
      }
      if ((value & (value - 1)) != 0) {
        FATAL("%d:%d: alignment must be power-of-two\n", t.range.start.line,
              t.range.start.col);
      }

      if (p >= end)
        return 0;
      if (*p != '.')
        return 0;
      p++;
      break;
    }

    default:
      return 0;
  }

  /* read mem type suffix */
  int i;
  for (i = 0; i < num_types; ++i) {
    const char* type = types[i];
    int type_len = strlen(type);
    if (p + type_len == end) {
      if (strncmp(p, type, type_len) == 0)
        return 1;
    }
  }

  return 0;
}

static void unexpected_token(Token t) {
  if (t.type == TOKEN_TYPE_EOF) {
    FATAL("%d:%d: unexpected EOF\n", t.range.start.line, t.range.start.col);
  } else {
    FATAL("%d:%d: unexpected token \"%.*s\"\n", t.range.start.line,
          t.range.start.col, (int)(t.range.end.pos - t.range.start.pos),
          t.range.start.pos);
  }
}

static void parse_generic(Tokenizer* tokenizer) {
  int nesting = 1;
  while (nesting > 0) {
    Token t = read_token(tokenizer);
    if (t.type == TOKEN_TYPE_OPEN_PAREN) {
      nesting++;
    } else if (t.type == TOKEN_TYPE_CLOSE_PAREN) {
      nesting--;
    } else if (t.type == TOKEN_TYPE_EOF) {
      FATAL("unexpected eof.\n");
    }
  }
}

static void parse_var(Tokenizer* tokenizer) {
  Token t = read_token(tokenizer);
  if (t.type == TOKEN_TYPE_ATOM) {
    if (t.range.end.pos - t.range.start.pos >= 1 &&
        t.range.start.pos[0] == '$') {
      /* var name */
    } else {
      /* var index */
      uint32_t value;
      const char* p = t.range.start.pos;
      if (!read_uint32(&p, t.range.end.pos, &value) || p != t.range.end.pos) {
        FATAL("%d:%d: invalid var index\n", t.range.start.line,
              t.range.start.col);
      }
      (void)value;
    }
  } else {
    unexpected_token(t);
  }
}

static void parse_type(Tokenizer* tokenizer, Type* type) {
  Token t = read_token(tokenizer);
  if (!match_type(t, type)) {
    FATAL("%d:%d: expected type\n", t.range.start.line, t.range.start.col);
  }
}

static void parse_expr(Tokenizer* tokenizer);

static void parse_type_list(Tokenizer* tokenizer) {
  Token t = read_token(tokenizer);
  while (1) {
    if (!match_type(t, NULL)) {
      unexpected_token(t);
    }
    t = read_token(tokenizer);
    if (t.type == TOKEN_TYPE_CLOSE_PAREN)
      break;
  }
}

static void parse_block(Tokenizer* tokenizer) {
  while (1) {
    parse_expr(tokenizer);
    Token t = read_token(tokenizer);
    if (t.type == TOKEN_TYPE_CLOSE_PAREN)
      break;
    rewind_token(tokenizer, t);
  }
}

static void parse_expr_list(Tokenizer* tokenizer) {
  while (1) {
    Token t = read_token(tokenizer);
    if (t.type == TOKEN_TYPE_CLOSE_PAREN)
      break;
    rewind_token(tokenizer, t);
    parse_expr(tokenizer);
  }
}

static void parse_const(Tokenizer* tokenizer, Type type) {
  Token t = read_token(tokenizer);
  expect_atom(t);
  const char* p = t.range.start.pos;
  const char* end = t.range.end.pos;
  switch (type) {
    case TYPE_I32: {
      uint32_t value;
      if (!read_uint32(&p, end, &value)) {
        FATAL("%d:%d: invalid unsigned 32-bit int\n", t.range.start.line,
              t.range.start.col);
      }
      break;
    }

    case TYPE_I64: {
      uint64_t value;
      if (!read_uint64(&p, end, &value)) {
        FATAL("%d:%d: invalid unsigned 64-bit int\n", t.range.start.line,
              t.range.start.col);
      }
      break;
    }

    case TYPE_F32:
    case TYPE_F64: {
      double value;
      if (!read_double(&p, end, &value)) {
        FATAL("%d:%d: invalid double\n", t.range.start.line, t.range.start.col);
      }
      break;
    }

    default:
      assert(0);
  }
}

static void parse_expr(Tokenizer* tokenizer) {
  Type type;
  Type in_type;
  expect_open(read_token(tokenizer));
  Token t = read_token(tokenizer);
  if (t.type == TOKEN_TYPE_ATOM) {
    if (match_atom(t, "nop")) {
      expect_close(read_token(tokenizer));
    } else if (match_atom(t, "block")) {
      parse_block(tokenizer);
    } else if (match_atom(t, "if")) {
      parse_expr(tokenizer); /* condition */
      parse_expr(tokenizer); /* true */
      t = read_token(tokenizer);
      if (t.type != TOKEN_TYPE_CLOSE_PAREN) {
        rewind_token(tokenizer, t);
        parse_expr(tokenizer); /* false */
        expect_close(read_token(tokenizer));
      }
    } else if (match_atom(t, "loop")) {
      parse_block(tokenizer);
    } else if (match_atom(t, "label")) {
      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_ATOM) {
        /* label */
        expect_var_name(t);
      } else if (t.type == TOKEN_TYPE_OPEN_PAREN) {
        /* no label */
        rewind_token(tokenizer, t);
      } else {
        unexpected_token(t);
      }
      parse_block(tokenizer);
    } else if (match_atom(t, "break")) {
      t = read_token(tokenizer);
      if (t.type != TOKEN_TYPE_CLOSE_PAREN) {
        rewind_token(tokenizer, t);
        parse_var(tokenizer);
        expect_close(read_token(tokenizer));
      }
    } else if (match_atom_prefix(t, "switch")) {
      /* TODO(binji) */
    } else if (match_atom(t, "call")) {
      parse_var(tokenizer);
      parse_expr_list(tokenizer);
    } else if (match_atom(t, "dispatch")) {
      parse_var(tokenizer);
    } else if (match_atom(t, "return")) {
      parse_expr_list(tokenizer);
    } else if (match_atom(t, "destruct")) {
      parse_var(tokenizer);
    } else if (match_atom(t, "getparam")) {
      parse_var(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_atom(t, "getlocal")) {
      parse_var(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_atom(t, "setlocal")) {
      parse_var(tokenizer);
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_atom(t, "load_global")) {
      parse_var(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_atom(t, "store_global")) {
      parse_var(tokenizer);
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_load_store(t, "load")) {
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_load_store(t, "store")) {
      parse_expr(tokenizer);
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_const(t, &type)) {
      parse_const(tokenizer, type);
      expect_close(read_token(tokenizer));
    } else if (match_unary(t, &type)) {
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_binary(t, &type)) {
      parse_expr(tokenizer);
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_compare(t, &in_type, &type)) {
      parse_expr(tokenizer);
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_convert(t, &in_type, &type)) {
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else if (match_cast(t, &in_type, &type)) {
      parse_expr(tokenizer);
      expect_close(read_token(tokenizer));
    } else {
      unexpected_token(t);
    }
  }
}

static void parse_func(Tokenizer* tokenizer) {
  Token t = read_token(tokenizer);
  if (t.type == TOKEN_TYPE_ATOM) {
    /* named function */
    t = read_token(tokenizer);
  }

  while (1) {
    if (t.type == TOKEN_TYPE_OPEN_PAREN) {
      Token open = t;
      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_ATOM) {
        if (match_atom(t, "param")) {
          t = read_token(tokenizer);
          rewind_token(tokenizer, t);
          if (match_type(t, NULL)) {
            parse_type_list(tokenizer);
          } else {
            expect_var_name(read_token(tokenizer));
            parse_type(tokenizer, NULL);
            expect_close(read_token(tokenizer));
          }
        } else if (match_atom(t, "result")) {
          parse_type_list(tokenizer);
        } else if (match_atom(t, "local")) {
          t = read_token(tokenizer);
          rewind_token(tokenizer, t);
          if (match_type(t, NULL)) {
            parse_type_list(tokenizer);
          } else {
            expect_var_name(read_token(tokenizer));
            parse_type(tokenizer, NULL);
            expect_close(read_token(tokenizer));
          }
        } else {
          rewind_token(tokenizer, open);
          parse_expr(tokenizer);
        }
      } else {
        unexpected_token(t);
      }
      t = read_token(tokenizer);
    } else if (t.type == TOKEN_TYPE_CLOSE_PAREN) {
      break;
    } else {
      unexpected_token(t);
    }
  }
}

static void preparse_func(Tokenizer* tokenizer, Module* module) {
  parse_generic(tokenizer);
}

static void preparse_global(Tokenizer* tokenizer, Module* module) {
  Token t = read_token(tokenizer);
  Type type;
  if (match_type(t, &type)) {
    while (1) {
      realloc_list((void**)&module->globals, &module->num_globals,
                   sizeof(Global));
      Global* global = &module->globals[module->num_globals - 1];
      global->name = NULL;
      global->type = type;

      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_CLOSE_PAREN)
        break;
      else if (!match_type(t, &type))
        unexpected_token(t);
    }
  } else {
    expect_var_name(t);
    parse_type(tokenizer, &type);
    expect_close(read_token(tokenizer));

    const char* name =
        strndup(t.range.start.pos, t.range.end.pos - t.range.start.pos);
    if (get_global_by_name(module, name) != -1) {
      FATAL("%d:%d: redefinition of global \"%s\"\n", t.range.start.line,
            t.range.start.col, name);
    }

    realloc_list((void**)&module->globals, &module->num_globals,
                 sizeof(Global));
    Global* global = &module->globals[module->num_globals - 1];
    global->name = name;
    global->type = type;
  }
}

static void preparse_module(Tokenizer* tokenizer, Module* module) {
  Token t = read_token(tokenizer);
  Token first = t;
  while (1) {
    if (t.type == TOKEN_TYPE_OPEN_PAREN) {
      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_ATOM) {
        if (match_atom(t, "func")) {
          preparse_func(tokenizer, module);
        } else if (match_atom(t, "global")) {
          preparse_global(tokenizer, module);
        } else {
          parse_generic(tokenizer);
        }
      } else {
        unexpected_token(t);
      }
      t = read_token(tokenizer);
    } else if (t.type == TOKEN_TYPE_CLOSE_PAREN) {
      break;
    } else {
      unexpected_token(t);
    }
  }
  rewind_token(tokenizer, first);
}

static void parse_module(Tokenizer* tokenizer) {
  Module module = {};
  preparse_module(tokenizer, &module);

  Token t = read_token(tokenizer);
  while (1) {
    if (t.type == TOKEN_TYPE_OPEN_PAREN) {
      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_ATOM) {
        if (match_atom(t, "func")) {
          parse_func(tokenizer);
        } else if (match_atom(t, "global")) {
          t = read_token(tokenizer);
          if (match_type(t, NULL)) {
            rewind_token(tokenizer, t);
            parse_type_list(tokenizer);
          } else {
            expect_var_name(t);
            parse_type(tokenizer, NULL);
            expect_close(read_token(tokenizer));
          }
        } else if (match_atom(t, "export")) {
          parse_generic(tokenizer);
        } else if (match_atom(t, "table")) {
          parse_generic(tokenizer);
        } else if (match_atom(t, "memory")) {
          parse_generic(tokenizer);
        } else {
          unexpected_token(t);
        }
      } else {
        unexpected_token(t);
      }
      t = read_token(tokenizer);
    } else if (t.type == TOKEN_TYPE_CLOSE_PAREN) {
      break;
    } else {
      unexpected_token(t);
    }
  }
}

static void parse(Tokenizer* tokenizer) {
  Token t = read_token(tokenizer);
  while (1) {
    if (t.type == TOKEN_TYPE_OPEN_PAREN) {
      t = read_token(tokenizer);
      if (t.type == TOKEN_TYPE_ATOM) {
        if (match_atom(t, "module")) {
          parse_module(tokenizer);
        } else if (match_atom(t, "asserteq")) {
          parse_generic(tokenizer);
        } else if (match_atom(t, "invoke")) {
          parse_generic(tokenizer);
        } else if (match_atom(t, "assertinvalid")) {
          parse_generic(tokenizer);
        } else {
          unexpected_token(t);
        }
      } else {
        unexpected_token(t);
      }
      t = read_token(tokenizer);
    } else if (t.type == TOKEN_TYPE_EOF) {
      break;
    } else {
      unexpected_token(t);
    }
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s [file.wasm]\n", argv[0]);
    return 0;
  }

  const char* filename = argv[1];
  FILE* f = fopen(filename, "rb");
  if (!f) {
    FATAL("unable to read %s\n", filename);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  void* data = malloc(fsize);
  if (!data) {
    FATAL("unable to alloc %zd bytes\n", fsize);
  }
  if (fread(data, 1, fsize, f) != fsize) {
    FATAL("unable to read %zd bytes from %s\n", fsize, filename);
  }
  fclose(f);

  Source source;
  source.start = data;
  source.end = data + fsize;

  Tokenizer tokenizer;
  tokenizer.source = source;
  tokenizer.loc.pos = source.start;
  tokenizer.loc.line = 1;
  tokenizer.loc.col = 1;

  parse(&tokenizer);
  return 0;
}
