this_file := $(abspath $(lastword $(MAKEFILE_LIST)))
COMMIT ?= HEAD
ARCHIVE ?= latest.tar
.PHONY: archive_deps


default: $(ARCHIVE).xz

# this rule archives every submodule, taking care of the correct path
# of each archive.
archive_submodules: archive_deps
	rm -f $(ARCHIVE).xz
	git archive --prefix=$(PREFIX) -o $(ARCHIVE) $(COMMIT) --add-file=archive_deps --add-file=$(this_file)
	tar r --file=$(ARCHIVE) $(this_file) ; \
	git submodule foreach 'make -f $(this_file) ARCHIVE=latest.tar  COMMIT=$$sha1 PREFIX=$(PREFIX)$$sm_path/ archive_submodules || :'


$(ARCHIVE).xz: archive_submodules concat_submodules 
	xz --keep $(ARCHIVE)
# ensure the list of submodules is available after archiving
# everything
archive_deps:
	git submodule foreach -q 'echo $$path'> archive_deps

# this rule stitches the archives together, recursively.
concat_submodules: 
	echo concating submodules;
	for dep in $$(cat archive_deps); do \
	cd $$dep; echo concating $$dep; \
	make -f $(this_file) concat_submodules; cd ../; \
	tar --file=$(ARCHIVE) --concatenate $$dep/latest.tar; \
	done

make_all:
	for dep in $$(cat archive_deps); do \
	cd $$dep; echo making $$dep; \
	make -f $(this_file) make_all; \
	cd ../; \
	done
	make
