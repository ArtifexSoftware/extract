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
test: test-mu test-gs

test-mu: Python2.pdf-test-mu zlib.3.pdf-test-mu

%.pdf-test-mu: %.pdf $(exe)
	@echo
	@echo === Testing $<
	mkdir -p test
	@echo == Generating intermediate with mutool.
	../mupdf/build/debug/mutool draw -F raw -o test/$<.mu-raw.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -c test/$<.mu-raw.content.xml -m raw -i test/$<.mu-raw.intermediate.xml -o test/$<.mu-raw.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u test/$<.mu-raw.content.xml $<.mu-raw.content.ref.xml
	@echo == Test succeeded.

test-gs: zlib.3.pdf-test-gs Python2.pdf-test-gs

%.pdf-test-gs: %.pdf $(exe)
	@echo
	@echo === Testing $<
	mkdir -p test
	@echo == Generating intermediate with gs.
	../ghostpdl/debug-bin/gs -sDEVICE=txtwrite -dTextFormat=4 -o test/$<.gs.intermediate.xml $<
	@echo == Generating output.
	./$(exe) -c test/$<.gs.content.xml -m raw -i test/$<.gs.intermediate.xml -o test/$<.gs.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	#diff -u test/$<.gs.content.xml $<.gs.content.ref.xml
	@echo == Test succeeded.

%.pdf-test2-mu: %.pdf $(exe)
	./$(exe) -m raw -i test/$<.mu-raw.intermediate2.xml -o test/$<.mu-raw2.docx -p 1 -t template.docx

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
