#
# Copyright 2016 WebAssembly Community Group participants
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

.SUFFIXES:

ALL = sexpr-wasm
EVERYTHING = $(ALL) sexpr-wasm-asan sexpr-wasm-msan sexpr-wasm-lsan
CFLAGS = -Wall -Werror -g -Wno-unused-function -Wno-return-type
DEPEND_FLAGS = -MMD -MP -MF $(patsubst %.o,%.d,$@)
LIBS =

AFL_DIR ?= ~/dev/afl/afl-1.83b
AFL_CC := $(AFL_DIR)/afl-gcc

SEXPR_WASM_CC := $(CC)
SEXPR_WASM_CFLAGS := $(CFLAGS)
SEXPR_WASM_SRCS := \
	wasm.c sexpr-wasm.c wasm-bison-parser.c wasm-flex-lexer.c wasm-vector.c \
	wasm-check.c wasm-writer.c wasm-binary-writer.c wasm-allocator.c \
	wasm-stack-allocator.c wasm-ast.c

ASAN_FLAGS := -fsanitize=address
SEXPR_WASM_ASAN_CC := clang
SEXPR_WASM_ASAN_CFLAGS := $(ASAN_FLAGS) $(CFLAGS)
SEXPR_WASM_ASAN_LDFLAGS := $(ASAN_FLAGS)
SEXPR_WASM_ASAN_SRCS := $(SEXPR_WASM_SRCS)

MSAN_FLAGS := -fsanitize=memory
SEXPR_WASM_MSAN_CC := clang
SEXPR_WASM_MSAN_CFLAGS := $(MSAN_FLAGS) $(CFLAGS)
SEXPR_WASM_MSAN_LDFLAGS := $(MSAN_FLAGS)
SEXPR_WASM_MSAN_SRCS := $(SEXPR_WASM_SRCS)

LSAN_FLAGS := -fsanitize=leak
SEXPR_WASM_LSAN_CC := clang
SEXPR_WASM_LSAN_CFLAGS := $(LSAN_FLAGS) $(CFLAGS)
SEXPR_WASM_LSAN_LDFLAGS := $(LSAN_FLAGS)
SEXPR_WASM_LSAN_SRCS := $(SEXPR_WASM_SRCS)

SEXPR_WASM_FUZZ_CC := $(AFL_CC)
SEXPR_WASM_FUZZ_CFLAGS := -O3 $(CFLAGS)
SEXPR_WASM_FUZZ_LDFLAGS :=
SEXPR_WASM_FUZZ_SRCS := $(SEXPR_WASM_SRCS)

.PHONY: all
all: $(addprefix out/,$(ALL))

.PHONY: everything
everything: $(addprefix out/,$(EVERYTHING))

out:
	mkdir -p $@

src/wasm-flex-lexer.c:  src/wasm-flex-lexer.l
	flex -o src/wasm-flex-lexer.c $<

src/wasm-bison-parser.c src/wasm-bison-parser.h: src/wasm-bison-parser.y
	bison -o src/wasm-bison-parser.c --defines=src/wasm-bison-parser.h $<

define EXE
$(2)_OBJS = $$(patsubst %.c,out/obj/$(1)/%.o,$$($(2)_SRCS))

out/obj/$(1):
	mkdir -p $$@

$$($(2)_OBJS): out/obj/$(1)/%.o: src/%.c | out/obj/$(1)
	$$($(2)_CC) $$($(2)_CFLAGS) -c -o $$@ $$(DEPEND_FLAGS) $$<

out/$(1): $$($(2)_OBJS) | out
	$$($(2)_CC) $$($(2)_LDFLAGS) -o $$@ $$^ $${LIBS}

-include $$($(2)_OBJS:.o=.d)
endef

$(eval $(call EXE,sexpr-wasm,SEXPR_WASM))
$(eval $(call EXE,sexpr-wasm-asan,SEXPR_WASM_ASAN))
$(eval $(call EXE,sexpr-wasm-msan,SEXPR_WASM_MSAN))
$(eval $(call EXE,sexpr-wasm-lsan,SEXPR_WASM_LSAN))
$(eval $(call EXE,sexpr-wasm-fuzz,SEXPR_WASM_FUZZ))

#### TESTS ####
.PHONY: test
test: out/sexpr-wasm
	@python test/run-tests.py

.PHONY: test-asan
test-asan: out/sexpr-wasm-asan
	@python test/run-tests.py -e $<

.PHONY: test-msan
test-msan: out/sexpr-wasm-msan
	@python test/run-tests.py -e $<

.PHONY: test-lsan
test-lsan: out/sexpr-wasm-lsan
	@python test/run-tests.py -e $<

.PHONY: test-everything
test-everything: test test-asan test-msan test-lsan

#### FUZZ ####

FUZZ_IN = fuzz-in
FUZZ_OUT = fuzz-out

.PHONY: fuzz
fuzz: out/sexpr-wasm-fuzz
	$(AFL_DIR)/afl-fuzz -i $(FUZZ_IN) -o $(FUZZ_OUT) -x wasm.dict -- $^ @@

#### CLEAN ####
.PHONY: clean
clean:
	rm -rf out
