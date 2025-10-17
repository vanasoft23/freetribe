ifndef MAKEFLAGS
CPUS ?= $(shell nproc)
MAKEFLAGS += -j $(CPUS) -l $(CPUS) -s
endif

all:
	mkdir -p ./dsp/src/common
	cd ./dsp && $(MAKE)
	cd ./dsp/build && xxd -i bfin.ldr > bfin_ldr.h
	cp -u ./dsp/build/bfin_ldr.h ./cpu/src/resources/bfin_ldr.h
	cd ./cpu && $(MAKE)
	rm -rf ./dsp/src/common

asm:
	mkdir -p ./dsp/src/common
	cd ./dsp && $(MAKE) asm
	cd ./dsp/build && xxd -i bfin.ldr > bfin_ldr.h
	cp -u ./dsp/build/bfin_ldr.h ./cpu/src/resources/bfin_ldr.h
	cd ./cpu && $(MAKE) asm
	rm -rf ./dsp/src/common

clean:
	cd ./dsp && $(MAKE) clean
	cd ./cpu && $(MAKE) clean
