SHELL = bash

all: directories.txt

#
# The order of directories in the documentation can be expressed as dependencies.
#
../demo: ../base

directories.txt: $(shell find .. -name README) Makefile
	@echo "creating $@"
	@echo "Directory structure of the Genode source tree" > $@
	@echo "#############################################" >> $@
	@echo >> $@
	@$(MAKE) --no-print-directory .. >> $@

DIRECTORIES = $(shell find .. -type d)

.PHONY: $(DIRECTORIES)

$(DIRECTORIES):
	@if test -r $@/README; then \
	  dir=$@; dir="$${dir:3}"; \
	  if test -n "$$dir"; then \
	    dir="'$$dir'"; \
	    echo "$$dir"; \
	    echo "$${dir//?/=}"; \
	    echo; \
	  fi; \
	  cat $@/README; \
	  echo; \
	  echo; \
	fi; \
	recursion="$(shell find $@/* -mindepth 1 -name README -printf '%H\n')"; \
	if test "$$recursion"; then $(MAKE) --no-print-directory -s $$recursion; fi

clean cleanall:
	rm -f directories.txt
