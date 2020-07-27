# Example commands:
#   make build=debug test
#   make build=debug-opt
#   make build=opt
#


# Build flags.
#
flags_link      = -W -Wall -lm
flags_compile   = -W -Wall -MMD -MP

ifeq ($(build),debug)
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
  $(error unrecognised $$(build) = $(build))
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
test: Python2.pdf-test zlib.3.pdf-test

%.pdf-test: %.pdf $(exe)
	@echo
	@echo == Testing $<
	mkdir -p test
	@echo == Generating intermediate with mutool.s
	../mupdf/build/debug/mutool draw -F raw -o test/$<.mu-raw-intermediate.xml $<
	@echo == Generating output.
	./$(exe) -c test/$<.content.xml -m raw -i test/$<.mu-raw-intermediate.xml -o test/$<.raw.docx -p 1 -t template.docx
	@echo == Comparing output with reference output.
	diff -u $<.content.ref.xml test/$<.content.xml
	@echo == Test succeeded.


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

# Dynamic dependencies.
#
-include $(dep)
