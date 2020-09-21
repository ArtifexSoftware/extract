# Example commands:
#
#   make
#   make tests
#       runs all tests.
#
#   make build=debug-opt ...
#       set build flags.
#
#   make test-buffer
#       run buffer unit tests.
#


# Build flags.
#
build = debug

flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -Wmissing-declarations -Wmissing-prototypes -Werror -MMD -MP

ifeq ($(build),)
    $(error Need to specify build=debug|opt|debug-opt|memento)
else ifeq ($(build),debug)
    flags_link      += -g
    flags_compile   += -g
else ifeq ($(build),opt)
    flags_link      += -O2
    flags_compile   += -O2
else ifeq ($(build),debug-opt)
    flags_link      += -g -O2
    flags_compile   += -g -O2
else ifeq ($(build),memento)
    flags_link      += -g
    flags_compile   += -g -D MEMENTO
else
    $(error unrecognised $$(build)=$(build))
endif


# Locations of mutool and gs - we assume these are available at hard-coded
# paths.
#
gs      = ../ghostpdl/debug-bin/gs
mutool  = ../mupdf/build/debug/mutool


# Default target - run all tests.
#
tests-all: test-buffer test-misc tests


# Define the main test targets.
#
tests := test/Python2.pdf test/zlib.3.pdf
tests := $(patsubst test/%, test/generated/%, $(tests))
tests := \
        $(patsubst %, %.intermediate-mu.xml, $(tests)) \
        $(patsubst %, %.intermediate-gs.xml, $(tests)) \

tests := \
        $(patsubst %, %.content.xml,            $(tests)) \
        $(patsubst %, %.content-rotate.xml,     $(tests)) \
        $(patsubst %, %.content-autosplit.xml,  $(tests)) \

tests := $(patsubst %, %.diff, $(tests))

tests: $(tests)


# Main executable.
#
exe = src/build/extract-$(build).exe
exe_src = \
        src/astring.c \
        src/buffer.c \
        src/docx.c \
        src/docx_template.c \
        src/extract-exe.c \
        src/extract.c \
        src/outf.c \
        src/xml.c src/zip.c \

ifeq ($(build),memento)
    exe_src += src/memento.c
endif
exe_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_src))
exe_dep = $(exe_obj:.o=.d)
exe: $(exe)
$(exe): $(exe_obj)
	$(CC) $(flags_link) -o $@ $^ -lz -lm


# Define rules that make the various intermediate targets required by $(tests).
#
test/generated/%.pdf.intermediate-mu.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with mutool.
	@mkdir -p test/generated
	$(mutool) draw -F xmltext -o $@ $<

test/generated/%.pdf.intermediate-gs.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with gs.
	@mkdir -p test/generated
	$(gs) -sDEVICE=txtwrite -dTextFormat=4 -o $@ $<

%.content.xml: % $(exe)
	@echo
	@echo Generating content with extract.exe
	./$(exe) -r 0 -i $< --o-content $@ -o $@.docx

%.content-rotate.xml: % $(exe) Makefile
	@echo
	@echo Generating content with rotation with extract.exe
	./$(exe) -r 1 -s 0 -i $< --o-content $@ -o $@.docx

%.content-autosplit.xml: % $(exe)
	@echo
	@echo Generating content with autosplit with extract.exe
	./$(exe) -r 0 -i $< --autosplit 1 --o-content $@ -o $@.docx

test/generated/%.diff: test/generated/% test/%.ref
	@echo Comparing content with reference content.
	-diff -u $^ >$@


# Buffer unit test.
#
exe_buffer_test = src/build/buffer-test-$(build).exe
exe_buffer_test_src = src/buffer.c src/buffer-test.c src/outf.c
exe_buffer_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_buffer_test_src))
exe_buffer_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_buffer_test): $(exe_buffer_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-buffer: $(exe_buffer_test)
	@echo Running test-buffer
	./$<


# Misc unit test.
#
exe_misc_test = src/build/misc-test-$(build).exe
exe_misc_test_src = src/misc-test.c src/xml.c src/outf.c src/astring.c src/buffer.c
exe_misc_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_misc_test_src))
exe_misc_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_misc_test): $(exe_misc_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-misc: $(exe_misc_test)
	@echo Running test-misc
	./$<


# Zip analyse. For dev use only.
#
exe_ziptest = src/build/ziptest-$(build).exe
exe_ziptest_src = src/zip-test.c src/outf.c
exe_ziptest_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_ziptest_src))
exe_ziptest_dep = $(exe_ziptest_obj:.o=.d)
$(exe_ziptest): $(exe_ziptest_obj)
	$(CC) $(flags_link) -o $@ $^
ziptest: $(exe_ziptest)
	@echo Running ziptest
	#./$< test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx
	#./$< test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx.dir.docx
	./$< test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx 2>a
	./$< test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx.dir.docx 2>b
	#od -a test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx >aa
	#od -a test/generated/Python2.pdf.intermediate-mu.xml.content-rotate.xml.docx.dir.docx >bb


# Compile rule. We always include src/docx_template.c as a prerequisite in case
# code #includes docx_template.h.
#
src/build/%.c-$(build).o: src/%.c src/docx_template.c
	@mkdir -p src/build
	$(CC) -c $(flags_compile) -o $@ $<

# Rule for machine-generated source code, src/docx_template.c. Also generates
# src/docx_template.h.
#
# These files are also in git to allow builds if python is not available.
#
src/docx_template.c: src/docx_template_build.py .ALWAYS
	@echo Building $@
	./src/docx_template_build.py -i src/template.docx -o src/docx_template
.ALWAYS:
.PHONY: .ALWAYS

# Tell make to preserve all intermediate files.
#
.SECONDARY:


# Rule for tags.
#
tags: .ALWAYS
	ectags -R --extra=+fq --c-kinds=+px .


# Clean rule.
#
.PHONY: clean
clean:
	-rm -r src/build test/generated
.PHONY: clean


# Include dynamic dependencies.
#
# We use $(sort ...) to remove duplicates
#
dep = $(sort $(exe_dep) $(exe_buffer_test_dep) $(exe_misc_test_dep) $(exe_ziptest_dep))

-include $(dep)
