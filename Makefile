# Example commands:
#   make build=debug test
#   make build=debug-opt
#   make build=opt
#


# Build flags.
#
build = debug
flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -MMD -MP

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


# Source code.
#
src = extract.c

ifeq ($(build),memento)
    src += memento.c
endif


# Build files.
#
exe = build/extract-$(build).exe
obj = $(src:.c=.c-$(build).o)
obj := $(addprefix build/, $(obj))
dep = $(obj:.o=.d)


# Test rules.
#
# We assume that mutool and gs are available at hard-coded paths.
#
test: test-mu test-gs test-mu-as

test-mu: Python2.pdf-test-mu zlib.3.pdf-test-mu
test-mu-as: Python2.pdf-test-mu-as zlib.3.pdf-test-mu-as

%.pdf-test-mu: %.pdf $(exe)
	@echo
	@echo === Testing $<
	mkdir -p test
	@echo == Generating intermediate with mutool.
	../mupdf/build/debug/mutool draw -F raw -o test/$<.mu-raw.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -m raw -i test/$<.mu-raw.intermediate.xml --o-content test/$<.mu-raw.content.xml -o test/$<.mu-raw.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.mu-raw.content.xml $<.mu-raw.content.ref.xml
	@echo == Test succeeded.

# Run extract.exe with --autosplit, to stress joining of spans. Compare with
# the default .ref file.
%.pdf-test-mu-as: %.pdf $(exe)
	@echo
	@echo === Testing $<
	mkdir -p test
	@echo == Generating intermediate with mutool.
	../mupdf/build/debug/mutool draw -F raw -o test/$<.mu-raw.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -m raw --autosplit 1 -i test/$<.mu-raw.intermediate.xml --o-content test/$<.mu-raw.as.content.xml -o test/$<.mu-raw.as.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.mu-raw.as.content.xml $<.mu-raw.content.ref.xml
	@echo == Test succeeded.

test-gs: zlib.3.pdf-test-gs #Python2.pdf-test-gs

%.pdf-test-gs: %.pdf $(exe)
	@echo
	@echo === Testing $<
	mkdir -p test
	@echo == Generating intermediate with gs.
	../ghostpdl/debug-bin/gs -sDEVICE=txtwrite -dTextFormat=4 -o test/$<.gs.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -m gs -i test/$<.gs.intermediate.xml --o-content test/$<.gs.content.xml -o test/$<.gs.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.gs.content.xml $<.gs.content.ref.xml
	@echo == Test succeeded.

%.pdf-test2-mu: %.pdf $(exe)
	./$(exe) -m raw -i test/$<.mu-raw.intermediate2.xml -o test/$<.mu-raw2.docx -p 1 -t template.docx

%.pdf-test4-mu: %.pdf $(exe)
	./$(exe) -m raw -i test/$<.mu-raw.intermediate4.xml --scale 0.0 -c test/$<.mu-raw4.content.xml -o test/$<.mu-raw4.docx -p 1 -t template.docx 2>&1 | tee test/$<.mu-raw4.out
%.pdf-test4as-mu: %.pdf $(exe)
	./$(exe) -m raw -i test/$<.mu-raw.intermediate4.xml --scale 1.0 -c test/$<.mu-raw4as.content.xml -o test/$<.mu-raw4as.docx -p 1 -t template.docx --autosplit 0 2>&1 | tee test/$<.mu-raw4as.out

test-as: Python2.pdf-test4-mu Python2.pdf-test4as-mu
	diff -u test/Python2.pdf.mu-raw4.content.xml test/Python2.pdf.mu-raw4as.content.xml

# Build rules.
#
$(exe): $(obj)
	mkdir -p build
	cc $(flags_link) -o $@ $^

build/%.c-$(build).o: %.c
	mkdir -p build
	cc -c $(flags_compile) -o $@ $<


# Clean rule.
#
.PHONY: clean
clean:
	rm $(obj) $(dep) $(exe)

clean-all:
	rm -r build test 


web:
	rsync -ai test/*.docx *.pdf julian@casper.ghostscript.com:public_html/extract/

# Dynamic dependencies.
#
-include $(dep)
