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
else
  $(error unrecognised $$(build) = $(build))
endif

src = extract.c

exe = extract-$(build).exe
obj = $(src:.c=.c-$(build).o)
dep = $(src:.c=.c-$(build).d)


$(exe): $(obj)
	cc $(flags_link) -o $@ $^

%.c-$(build).o: %.c
	cc -c $(flags_compile) -o $@ $<

.PHONY: clean
clean:
	rm $(obj) $(dep) $(exe)

-include $(dep)
