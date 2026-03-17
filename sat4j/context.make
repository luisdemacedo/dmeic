# This file does a context switch to the context named
# 'remote'. Actually the name of the context is the name of the
# makefile with the .make extension, to simplify things.

# First, take care of dependencies,
deps := $(wildcard ./deps/*)
# Then, take care of myself.

cont: cont_deps cont_self 

cont_self:
	@if [ -f $(context).make ]; \
		then make -f $(context).make; fi

cont_deps: 
	@for dep in $(deps);\
		 do \
			if [ -f $$dep/$(context).make ]; \
			then make -C $$dep -f context.make context=$(context) ; \
			else echo "$$dep context not defined" ; fi ; done

