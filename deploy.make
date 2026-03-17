depRoot = deps
deployTarget = deployTarget

.PHONY: compress

depsList := $(wildcard ./depsList/*/)

compress: deploy
	tar -cJvf $(deployTarget).tar.xz $(deployTarget)

deploy: deploy_deps deployMe


deploy_deps: clean
	@for dep in $(depsList);\
	    do commit=$$(basename $$dep);\
	    echo $$commit;\
	    pushd $$dep; git stash -a -m "deploy"; git checkout "$$commit"; \
	    if [ -f ./deploy.make ]; \
	    then make -f ./deploy.make deploy; popd; \
            dep=$$(readlink -f $$dep);\
	    rsync -ruvapb --delete $$dep/$(deployTarget)/ ./$(depRoot)/`basename $$dep`; \
	    pushd $$dep;  make -f ./deploy.make clean; \git switch - ; git stash pop ; popd; \
	    else echo "How can I deploy $$dep?" ; exit 1; fi ; done

# look for a local filter file
ifneq ($(wildcard .git/info/exclude),)
 filter=.git
else
 ifeq ($(wildcard .git/.),)
  ifneq ($(wildcard .git),)  
   file=$(lastword $(file < .git))
   ifneq ($(wildcard $(file))/info/exclude,)  
   filter=$(file)
   endif
 endif
endif
endif
ifdef filter
 filter:=--exclude-from=$(filter)/info/exclude
endif

deployMe: half_deploy 
	@echo excluding with $(filter)
	@if [ -f deployFilter.rsync ]; \
		then rsync  --delete -ruvapb --filter=": deployFilter.rsync" $(filter) . ./deployTarget; \
		else echo "please provide deployFilter.rsync for $(basename CURDIR)"; exit 1 ; fi 
	@if [ -d deps ]; \
		then rsync --delete -ruvapb ./deps ./deployTarget ; fi 



clean:
	rm -rf deps/*
	rm -rf deployTarget/

half_deploy:
	@if [ -f halfDeploy.make ]; \
		then make -f halfDeploy.make half_deploy; \
		else echo "no half deploy file"; fi 

