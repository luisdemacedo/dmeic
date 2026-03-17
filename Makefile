# settings pertaining to the cplex libraries
default: build 

.PHONY: archive
# Define which solver to use as backend, this can be a name of a file in the
# solvers directory.
SOLVER     ?= glucose4.1
#
# The following values should be defined in the included file:
# VERSION    = core or simp 
# SOLVERNAME = name of the SAT solver
# SOLVERDIR  = subdirectory of the SAT solver
# NSPACE     = namespace of the SAT solver
#

# dependencies
.PHONY: build_deps build_submodules build clean_deps touch_cplex

build: s
all: build


# solver
include $(PWD)/solvers/$(SOLVER).mk
# THE REMAINING OF THE MAKEFILE SHOULD BE LEFT UNCHANGED
EXEC       = open-wbo
DEPDIR     += mtl utils core
DEPDIR     +=  ../../encodings ../../algorithms ../../graph ../../classifier ../../analysis ../../debug ../../waiting_list
MROOT      ?= $(PWD)/solvers/$(SOLVERDIR)
LFLAGS     += -lgmpxx -lgmp
CFLAGS     += -Wall -Wno-parentheses -Wno-class-memaccess -std=c++17 -DNSPACE=$(NSPACE) -DSOLVERNAME=$(SOLVERNAME) -DVERSION=$(VERSION) -fopenmp
ifeq ($(SANITIZER),asan)
CFLAGS     += -fsanitize=address
LFLAGS     += -fsanitize=address
LFLAGS     += -fuse-ld=gold
endif
ifeq ($(SANITIZER),undef)
CFLAGS     += -fsanitize=undefined -fsanitize-undefined-trap-on-error
LFLAGS     += -fsanitize=undefined
LFLAGS     += -fuse-ld=gold
endif
ifeq ($(VERSION),simp)
DEPDIR     += simp
CFLAGS     += -DSIMP=1 
ifeq ($(SOLVERDIR),glucored)
LFLAGS     += -pthread
CFLAGS     += -DGLUCORED
DEPDIR     += reducer glucored
endif
endif

# Some solvers do not have a template.mk file any more
# E.g.: Minisat or Riss
ifeq ($(SOLVERDIR),$(filter $(SOLVERDIR),minisat riss))
include $(PWD)/mtl/template.mk
else
include $(MROOT)/mtl/template.mk
endif
