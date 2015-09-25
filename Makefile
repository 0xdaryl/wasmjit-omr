.SUFFIXES:

ALL = sexpr-wasm
EVERYHING = $(ALL) sexpr-wasm-asan sexpr-wasm-msan sexpr-wasm-lsan
CFLAGS = -Wall -Werror -g
DEPEND_FLAGS = -MMD -MP -MF $(patsubst %.o,%.d,$@)
ASAN_FLAGS = -fsanitize=address
MSAN_FLAGS = -fsanitize=memory
LSAN_FLAGS = -fsanitize=leak

SRCS = sexpr-wasm.c wasm-parse.c wasm-gen.c
OBJS = $(addprefix out/,$(patsubst %.c,%.o,$(SRCS)))
ASAN_OBJS = $(addprefix out/,$(patsubst %.c,%.asan.o,$(SRCS)))
MSAN_OBJS = $(addprefix out/,$(patsubst %.c,%.msan.o,$(SRCS)))
LSAN_OBJS = $(addprefix out/,$(patsubst %.c,%.lsan.o,$(SRCS)))

.PHONY: all
all: $(addprefix out/,$(ALL))

.PHONY: everything
everything: $(addprefix out/,$(EVERYHING))

out/:
	mkdir $@

$(OBJS): out/%.o: src/%.c | out
	$(CC) $(CFLAGS) -c -o $@ $(DEPEND_FLAGS) $<
out/sexpr-wasm: $(OBJS) | out
	$(CC) -o $@ $^

# ASAN
$(ASAN_OBJS): out/%.asan.o: src/%.c | out
	clang $(ASAN_FLAGS) $(CFLAGS) -c -o $@ $(DEPEND_FLAGS) $<
out/sexpr-wasm-asan: $(ASAN_OBJS) | out
	clang $(ASAN_FLAGS) -o $@ $^

# MSAN
$(MSAN_OBJS): out/%.msan.o: src/%.c | out
	clang $(MSAN_FLAGS) $(CFLAGS) -c -o $@ $(DEPEND_FLAGS) $<
out/sexpr-wasm-msan: $(MSAN_OBJS) | out
	clang $(MSAN_FLAGS) -o $@ $^

# LSAN
$(LSAN_OBJS): out/%.lsan.o: src/%.c | out
	clang $(LSAN_FLAGS) $(CFLAGS) -c -o $@ $(DEPEND_FLAGS) $<
out/sexpr-wasm-lsan: $(LSAN_OBJS) | out
	clang $(LSAN_FLAGS) -o $@ $^

src/hash.h: src/hash.txt
	gperf --compare-strncmp --readonly-tables --struct-type $< --output-file $@

-include $(OBJS:.o=.d) $(ASAN_OBJS:.o=.d) $(MSAN_OBJS:.o=.d) $(LSAN_OBJS:.o=.d)

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

#### CLEAN ####
.PHONY: clean
clean:
	rm -rf out
