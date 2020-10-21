# Example commands:
#
#   make
#   make tests
#       Runs all tests.
#
#   make build=debug-opt ...
#       Set build flags.
#
#   make test-buffer
#       Run buffer unit tests.
#
#   make build=memento maqueeze
#       Run memento squeeze test.


# Build flags.
#
# Note that OpenBSD's clang-8 appears to ignore -Wdeclaration-after-statement.
#
build = debug

flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -Wpointer-sign -Wmissing-declarations -Wmissing-prototypes -Wdeclaration-after-statement -Werror -MMD -MP

uname = $(shell uname)

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
    ifeq ($(uname),OpenBSD)
        flags_link += -L /usr/local/lib -l execinfo
    endif
    flags_compile   += -g -D MEMENTO
else
    $(error unrecognised $$(build)=$(build))
endif


# Locations of mutool and gs - we assume these are available at hard-coded
# paths.
#
gs      = ../ghostpdl/debug-bin/gs
mutool  = ../mupdf/build/debug-extract/mutool


# Default target - run all tests.
#
tests-all: test-buffer test-misc tests


# Define the main test targets.
#
pdfs = test/Python2.pdf test/zlib.3.pdf test/text_graphic_image.pdf
pdfs_generated = $(patsubst test/%, test/generated/%, $(pdfs))

# Generate targets that check all combinations of mu/gs and the various
# rotate/autosplit options of extract-exe.
#
tests_exe :=  \
        $(patsubst %, %.intermediate-mu.xml, $(pdfs_generated)) \
        $(patsubst %, %.intermediate-gs.xml, $(pdfs_generated)) \

tests_exe := \
        $(patsubst %, %.extract.docx,                   $(tests_exe)) \
        $(patsubst %, %.extract-rotate.docx,            $(tests_exe)) \
        $(patsubst %, %.extract-rotate-spacing.docx,    $(tests_exe)) \
        $(patsubst %, %.extract-autosplit.docx,         $(tests_exe)) \
        $(patsubst %, %.extract-template.docx,          $(tests_exe)) \

tests_exe := $(patsubst %, %.diff, $(tests_exe))

# Targets that test direct conversion with mutool. As of 2020-10-16, mutool
# uses rotate=true and spacing=true, so we diff with reference directory
# ...pdf.intermediate-mu.xml.extract-rotate-spacing.docx.dir.ref
#
tests_mutool := \
        $(patsubst %, %.mutool.docx.diff, $(pdfs_generated)) \
        $(patsubst %, %.mutool-norotate.docx.diff, $(pdfs_generated)) \

#$(warn $(pdfs_generated_intermediate_docx_diffs))
#$(warn $(tests))

tests: $(tests_exe) $(tests_mutool)

tests-exe: $(tests_exe)

# Checks output of mutool conversion from .pdf to .docx. Requires that mutool
# was built with extract as a third-party library. As of 2020-10-16 this
# requires mupdf/thirdparty/extract e.g. as a softlink to extract checkout.
#
tests-mutool: $(tests_mutool)


# Main executable.
#
exe = src/build/extract-$(build).exe
exe_src = \
        src/alloc.c \
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
    ifeq ($(uname),Linux)
        flags_compile += -D HAVE_LIBDL
        flags_link += -L ../libbacktrace/.libs -l backtrace -l dl
    endif
endif
exe_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_src))
exe_dep = $(exe_obj:.o=.d)
exe: $(exe)
$(exe): $(exe_obj)
	$(CC) $(flags_link) -o $@ $^ -lz -lm

run_exe = $(exe)
ifeq ($(build),memento)
    ifeq ($(uname),Linux)
        #run_exe = MEMENTO_ABORT_ON_LEAK=1 LD_LIBRARY_PATH=../libbacktrace/.libs $(exe)
        run_exe = LD_LIBRARY_PATH=../libbacktrace/.libs $(exe)
    endif
endif


# Rules that make the various intermediate targets required by $(tests).
#

test/generated/%.pdf.intermediate-mu.xml: test/%.pdf $(mutool)
	@echo
	@echo == Generating intermediate file for $< with mutool.
	@mkdir -p test/generated
	$(mutool) draw -F xmltext -o $@ $<

test/generated/%.pdf.intermediate-gs.xml: test/%.pdf $(gs)
	@echo
	@echo == Generating intermediate file for $< with gs.
	@mkdir -p test/generated
	$(gs) -sDEVICE=txtwrite -dTextFormat=4 -o $@ $<

%.extract.docx: % $(exe)
	@echo
	@echo Generating docx with extract.exe
	$(run_exe) -v 1 -r 0 -i $< -o $@

%.extract-rotate.docx: % $(exe) Makefile
	@echo
	@echo == Generating docx with rotation with extract.exe
	$(run_exe) -r 1 -s 0 -i $< -o $@

%.extract-rotate-spacing.docx: % $(exe) Makefile
	@echo
	@echo == Generating docx with rotation with extract.exe
	$(run_exe) -r 1 -s 1 -i $< -o $@

%.extract-autosplit.docx: % $(exe)
	@echo
	@echo == Generating docx with autosplit with extract.exe
	$(run_exe) -r 0 -i $< --autosplit 1 -o $@

%.extract-template.docx: % $(exe)
	@echo
	@echo == Generating docx using src/template.docx with extract.exe
	$(run_exe) -r 0 -i $< -t src/template.docx -o $@

test/generated/%.docx.diff: test/generated/%.docx.dir test/%.docx.dir.ref
	@echo
	@echo == Checking $<
	diff -ru $^

# This checks that -t src/template.docx gives identical results.
#
test/generated/%.extract-template.docx.diff: test/generated/%.extract-template.docx.dir test/%.extract.docx.dir.ref
	@echo
	@echo == Checking $<
	diff -ru $^

# Unzips .docx into .docx.dir/ directory.
%.docx.dir: %.docx
	(rm -r $@ || true)
	unzip -q -d $@ $<

# Prettyifies each .xml file within .docx.dir/ directory.
%.docx.dir.pretty: %.docx.dir
	(rm -r $@ $@- || true)
	cp -pr $< $@-
	./src/docx_template_build.py --docx-pretty $@-
	mv $@- $@

# Converts .pdf directly to .docx using mutool.
test/generated/%.pdf.mutool.docx: test/%.pdf
	@echo
	@echo Converting .pdf directly to .docx using mutool.
	$(mutool) convert -o $@ $<

test/generated/%.pdf.mutool-norotate.docx: test/%.pdf
	@echo
	@echo Converting .pdf directly to .docx using mutool.
	$(mutool) convert -O rotation=0,spacing=1 -o $@ $<

# Compares .docx from mutool with reference .docx.
#
# As of 2020-10-16, mutool uses rotate=true and
# spacing=true, so we diff with reference directory
# ...pdf.intermediate-mu.xml.extract-rotate-spacing.docx.dir.ref
#
test/generated/%.pdf.mutool.docx.diff: test/generated/%.pdf.mutool.docx.dir test/%.pdf.intermediate-mu.xml.extract-rotate-spacing.docx.dir.ref
	@echo
	@echo == Checking $<
	diff -ru $^

test/generated/%.pdf.mutool-norotate.docx.diff: test/generated/%.pdf.mutool-norotate.docx.dir test/%.pdf.intermediate-mu.xml.extract.docx.dir.ref
	@echo
	@echo == Checking $<
	diff -ru $^


# Valgrind test
#
valgrind: $(exe) test/generated/Python2.pdf.intermediate-mu.xml
	 valgrind --leak-check=full $(exe) -h -r 1 -s 0 -i test/generated/Python2.pdf.intermediate-mu.xml -o test/generated/valgrind-out.docx

# Memento tests.
#
ifeq ($(build),memento)
msqueeze: $(exe) test/generated/Python2.pdf.intermediate-mu.xml
	MEMENTO_SQUEEZEAT=1 $(run_exe) --alloc-exp-min 0 -r 1 -s 0 -i test/generated/Python2.pdf.intermediate-mu.xml -o test/generated/msqueeze-out.docx 2>&1 | src/memento.py -q 1 -o msqueeze-raw
mfailat: $(exe) test/generated/Python2.pdf.intermediate-mu.xml
	MEMENTO_FAILAT=61463 $(run_exe) --alloc-min-block-size 0 -r 1 -s 0 -i test/generated/Python2.pdf.intermediate-mu.xml -o test/generated/msqueeze-out.docx
endif


# Temporary rules for generating reference files.
#
#temp_ersdr = \
#        $(patsubst %, %.intermediate-mu.xml.extract-rotate-spacing.docx.dir.ref, $(pdfs)) \
#        $(patsubst %, %.intermediate-gs.xml.extract-rotate-spacing.docx.dir.ref, $(pdfs)) \
#
#temp: $(temp_ersdr)
#test/%.xml.extract-rotate-spacing.docx.dir.ref: test/generated/%.xml.extract-rotate-spacing.docx.dir
#	@echo
#	@echo copying $< to %@
#	rsync -ai $</ $@/


# Buffer unit test.
#
exe_buffer_test = src/build/buffer-test-$(build).exe
exe_buffer_test_src = src/buffer.c src/buffer-test.c src/outf.c src/alloc.c
ifeq ($(build),memento)
    exe_buffer_test_src += src/memento.c
endif
exe_buffer_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_buffer_test_src))
exe_buffer_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_buffer_test): $(exe_buffer_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-buffer: $(exe_buffer_test)
	@echo Running test-buffer
	./$<
test-buffer-valgrind: $(exe_buffer_test)
	@echo Running test-buffer with valgrind
	valgrind --leak-check=full ./$<


# Misc unit test.
#
exe_misc_test = src/build/misc-test-$(build).exe
exe_misc_test_src = \
        src/alloc.c \
        src/astring.c \
        src/buffer.c \
        src/misc-test.c \
        src/outf.c \
        src/xml.c \

ifeq ($(build),memento)
    exe_misc_test_src += src/memento.c
endif
exe_misc_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_misc_test_src))
exe_misc_test_dep = $(exe_buffer_test_obj:.o=.d)
$(exe_misc_test): $(exe_misc_test_obj)
	$(CC) $(flags_link) -o $@ $^
test-misc: $(exe_misc_test)
	@echo Running test-misc
	./$<


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
clean:
	-rm -r src/build test/generated

# Cleans test/generated except for intermediate files, which are slow to create
# (when using gs).
clean2:
	-rm -r test/generated/*.pdf.intermediate-*.xml.*
	-rm -r test/generated/*.pdf.mutool*.docx*
.PHONY: clean


# Include dynamic dependencies.
#
# We use $(sort ...) to remove duplicates
#
dep = $(sort $(exe_dep) $(exe_buffer_test_dep) $(exe_misc_test_dep) $(exe_ziptest_dep))

-include $(dep)
