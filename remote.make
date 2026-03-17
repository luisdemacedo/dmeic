.PHONY: configCPLEX.patch
build: configCPLEX.patch
	@echo "context switch in Open-WBO-MO-base"
	patch -N -r-  cplex.make -i configCPLEX.patch

configCPLEX.patch:
	@echo "patching cplex"
	cat confremCPLEX.patch > configCPLEX.patch
