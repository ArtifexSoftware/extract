# Example commands:
#   make build=debug
#   make build=debug-opt
#   make build=opt
#

#build = debug
#build = debug-opt

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

src = extract.c

ifeq ($(build),memento)
    src += memento.c
endif

exe = build/extract-$(build).exe
obj = $(src:.c=.c-$(build).o)
obj := $(addprefix build/, $(obj))
dep = $(obj:.o=.d)


$(exe): $(obj)
	mkdir -p build
	cc $(flags_link) -o $@ $^

build/%.c-$(build).o: %.c
	mkdir -p build
	cc -c $(flags_compile) -o $@ $<

clean-test:
	rm -r {zlib.3,Python2}.pdf-*

.PHONY: clean
clean:
	rm $(obj) $(dep) $(exe)

%.pdf-test: %.pdf $(exe)
	mkdir -p test
	../mupdf/build/debug/mutool draw -F raw -o test/$<.mu-raw-intermediate.xml $<
	./$(exe) -c test/$<.content.xml -m raw -i test/$<.mu-raw-intermediate.xml -o test/$<.raw.docx -p 1 -t template.docx
	diff -u $<.content.ref.xml test/$<.content.xml

test: Python2.pdf-test zlib.3.pdf-test

-include $(dep)
