# Example commands:
#   make
#   make test
#   make build=debug-opt
#   make build=opt test
#   make test-buffer
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


ifeq ($(build),memento)
    exe_src += src/memento.c
endif


# Build files.
#
exe = src/build/extract-$(build).exe
exe_src = src/extract-exe.c src/extract.c src/astring.c src/docx.c src/outf.c src/xml.c src/zip.c src/buffer.c
exe_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_src)) src/build/docx_template.c-$(build).o
exe_dep = $(exe_obj:.o=.d)

exe_buffer_test = src/build/buffer-test-$(build).exe
exe_buffer_test_src = src/buffer.c src/buffer-test.c src/outf.c
exe_buffer_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_buffer_test_src))
exe_buffer_test_dep = $(exe_buffer_test_obj:.o=.d)

exe_misc_test = src/build/misc-test-$(build).exe
exe_misc_test_src = src/misc-test.c src/xml.c src/outf.c src/astring.c src/buffer.c
exe_misc_test_obj = $(patsubst src/%.c, src/build/%.c-$(build).o, $(exe_misc_test_src))
exe_misc_test_dep = $(exe_buffer_test_obj:.o=.d)


# Locations of mutool and gs - we assume these are available at hard-coded
# paths.
#
gs      = ../ghostpdl/debug-bin/gs
mutool  = ../mupdf/build/debug/mutool


# Test targets and rules.
#
test_files = test/Python2.pdf test/zlib.3.pdf

test_targets_mu             = $(patsubst test/%, test/generated/%.mu.intermediate.xml.content.xml.diff,             $(test_files))
test_targets_mu_autosplit   = $(patsubst test/%, test/generated/%.mu.intermediate.xml.autosplit.content.xml.diff,   $(test_files))
test_targets_gs             = $(patsubst test/%, test/generated/%.gs.intermediate.xml.content.xml.diff,             $(test_files))

test: test-misc test-buffer test-extract

test-extract: test-extract-mu test-extract-mu-as test-extract-gs

test-extract-mu:    $(test_targets_mu)
test-extract-mu-as: $(test_targets_mu_autosplit)
test-extract-gs:    $(test_targets_gs)

test/generated/%.pdf.mu.intermediate.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with mutool.
	@mkdir -p test/generated
	$(mutool) draw -F xmltext -o $@ $<

test/generated/%.pdf.gs.intermediate.xml: test/%.pdf
	@echo
	@echo Generating intermediate file for $< with gs.
	@mkdir -p test/generated
	$(gs) -sDEVICE=txtwrite -dTextFormat=4 -o $@ $<

test/generated/%.intermediate.xml.content.xml: test/generated/%.intermediate.xml $(exe)
	@echo
	@echo Generating content with extract.exe
	./$(exe) -i $< --o-content $@ -o $<.docx

test/generated/%.intermediate.xml.autosplit.content.xml: test/generated/%.intermediate.xml $(exe)
	@echo
	@echo Generating content with extract.exe autosplit
	./$(exe) -i $< --autosplit 1 --o-content $@ -o $<.docx

test/generated/%.intermediate.xml.content.xml.diff: test/generated/%.intermediate.xml.content.xml test/%.content.ref.xml
	@echo Diffing content with reference output.
	diff -u $^ >$@

test/generated/%.intermediate.xml.autosplit.content.xml.diff: test/generated/%.intermediate.xml.autosplit.content.xml test/%.content.ref.xml
	@echo Diffing content with reference output.
	diff -u $^ >$@

test-buffer: $(exe_buffer_test)
	@echo Running test-buffer
	./$<

test-misc: $(exe_misc_test)
	@echo Running test-misc
	./$<

# Rule for executables.
#
exe: $(exe)
$(exe): $(exe_obj)
	cc $(flags_link) -o $@ $^ -lz -lm

$(exe_buffer_test): $(exe_buffer_test_obj)
	cc $(flags_link) -o $@ $^

$(exe_misc_test): $(exe_misc_test_obj)
	cc $(flags_link) -o $@ $^

# Compile rules. We always include src/build/docx_template.c as a prerequisite
# in case code #includes docx_template.h.
#
src/build/%.c-$(build).o: src/%.c src/build/docx_template.c
	@mkdir -p src/build
	cc -c $(flags_compile) -o $@ $<

src/build/%.c-$(build).o: src/build/%.c
	@mkdir -p src/build
	cc -c $(flags_compile) -o $@ $<

# Rule for machine-generated source code, src/build/docx_template.c. Also
# generates src/build/docx_template.h.
#
src/build/docx_template.c: .ALWAYS
	@echo Building $@
	@mkdir -p src/build
	./src/docx_template_build.py -i src/template.docx -o src/build/docx_template
.ALWAYS:


# Rule for tags
#
tags: .ALWAYS
	ectags -R --extra=+fq --c-kinds=+px .


# Clean rule.
#
.PHONY: clean
clean:
	-rm -r src/build test/generated


# Copy generated files to website.
#
web:
	rsync -ai test/generated/*.docx test/*.pdf julian@casper.ghostscript.com:public_html/extract/


# Dynamic dependencies.
#
# Use $(sort ...) to remove duplicates
dep = $(sort $(exe_dep) $(exe_buffer_test_dep))
-include $(dep)
