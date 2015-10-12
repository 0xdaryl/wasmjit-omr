#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "wasm.h"
#include "wasm-internal.h"
#include "wasm-parse.h"
#include "wasm-gen.h"

enum {
  FLAG_VERBOSE,
  FLAG_HELP,
  FLAG_DUMP_MODULE,
  FLAG_OUTPUT,
  FLAG_MULTI_MODULE,
  FLAG_MULTI_MODULE_VERBOSE,
  FLAG_TYPECHECK_SPEC,
  FLAG_TYPECHECK_V8,
  NUM_FLAGS
};

static const char* g_infile;
const char* g_outfile;
int g_dump_module;
int g_verbose;
WasmParserTypeCheck g_parser_type_check = WASM_PARSER_TYPE_CHECK_V8_NATIVE;
static int g_multi_module;
static int g_multi_module_verbose;

static struct option g_long_options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"dump-module", no_argument, NULL, 'd'},
    {"output", no_argument, NULL, 'o'},
    {"multi-module", no_argument, NULL, 0},
    {"multi-module-verbose", no_argument, NULL, 0},
    {"typecheck-spec", no_argument, NULL, 0},
    {"typecheck-v8", no_argument, NULL, 0},
    {NULL, 0, NULL, 0},
};
STATIC_ASSERT(NUM_FLAGS + 1 == ARRAY_SIZE(g_long_options));

typedef struct OptionHelp {
  int flag;
  const char* metavar;
  const char* help;
} OptionHelp;

static OptionHelp g_option_help[] = {
    {FLAG_VERBOSE, NULL, "use multiple times for more info"},
    {FLAG_DUMP_MODULE, NULL, "print a hexdump of the module to stdout"},
    {FLAG_MULTI_MODULE, NULL,
     "parse a file with multiple modules and assertions, like the spec tests"},
    {FLAG_MULTI_MODULE_VERBOSE, NULL,
     "print logging messages when running multi-module files"},
    {NUM_FLAGS, NULL},
};

static void usage(const char* prog) {
  printf("usage: %s [option] filename\n", prog);
  printf("options:\n");
  struct option* opt = &g_long_options[0];
  int i = 0;
  for (; opt->name; ++i, ++opt) {
    OptionHelp* help = NULL;

    int n = 0;
    while (g_option_help[n].help) {
      if (i == g_option_help[n].flag) {
        help = &g_option_help[n];
        break;
      }
      n++;
    }

    if (opt->val) {
      printf("  -%c, ", opt->val);
    } else {
      printf("      ");
    }

    if (help && help->metavar) {
      char buf[100];
      snprintf(buf, 100, "%s=%s", opt->name, help->metavar);
      printf("--%-30s", buf);
    } else {
      printf("--%-30s", opt->name);
    }

    if (help) {
      printf("%s", help->help);
    }

    printf("\n");
  }
  exit(0);
}

static void parse_options(int argc, char** argv) {
  int c;
  int option_index = 0;

  while (1) {
    c = getopt_long(argc, argv, "vhdo:", g_long_options, &option_index);
    if (c == -1) {
      break;
    }

  redo_switch:
    switch (c) {
      case 0:
        c = g_long_options[option_index].val;
        if (c) {
          goto redo_switch;
        }

        switch (option_index) {
          case FLAG_VERBOSE:
          case FLAG_HELP:
          case FLAG_DUMP_MODULE:
          case FLAG_OUTPUT:
            /* Handled above by goto */
            assert(0);
            break;

          case FLAG_MULTI_MODULE:
            g_multi_module = 1;
            break;

          case FLAG_MULTI_MODULE_VERBOSE:
            g_multi_module_verbose = 1;
            break;

          case FLAG_TYPECHECK_SPEC:
            g_parser_type_check = WASM_PARSER_TYPE_CHECK_SPEC;
            break;

          case FLAG_TYPECHECK_V8:
            g_parser_type_check = WASM_PARSER_TYPE_CHECK_V8_NATIVE;
            break;
        }
        break;

      case 'v':
        g_verbose++;
        break;

      case 'h':
        usage(argv[0]);

      case 'd':
        g_dump_module = 1;
        break;

      case 'o':
        g_outfile = optarg;
        break;

      case '?':
        break;

      default:
        FATAL("getopt_long returned '%c' (%d)\n", c, c);
        break;
    }
  }

  if (optind < argc) {
    g_infile = argv[optind];
  } else {
    FATAL("No filename given.\n");
    usage(argv[0]);
  }
}

int main(int argc, char** argv) {
  parse_options(argc, argv);

  FILE* f = fopen(g_infile, "rb");
  if (!f) {
    FATAL("unable to read %s\n", g_infile);
  }

  fseek(f, 0, SEEK_END);
  size_t fsize = ftell(f);
  fseek(f, 0, SEEK_SET);
  void* data = malloc(fsize);
  if (!data) {
    FATAL("unable to alloc %zd bytes\n", fsize);
  }
  if (fread(data, 1, fsize, f) != fsize) {
    FATAL("unable to read %zd bytes from %s\n", fsize, g_infile);
  }
  fclose(f);

  WasmSource source;
  source.filename = g_infile;
  source.start = data;
  source.end = data + fsize;

  WasmGenOptions options = {};
  options.multi_module = g_multi_module;
  options.multi_module_verbose = g_multi_module_verbose;
  options.type_check = g_parser_type_check;
  int result = wasm_gen_file(&source, &options);
  free(data);
  return result;
}
