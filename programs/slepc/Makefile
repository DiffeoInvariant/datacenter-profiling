

.PHONY: svd parsetest webserver webserver_launcher

#default: all

#all: svd
petsc.pc := $(PETSC_DIR)/$(PETSC_ARCH)/lib/pkgconfig/petsc.pc
PACKAGES := $(petsc.pc)
CC := $(shell pkg-config --variable=ccompiler $(PACKAGES))
CFLAGS_OTHER := $(shell pkg-config --cflags-only-other $(PACKAGES))
CFLAGS := $(shell pkg-config --variable=cflags_extra $(PACKAGES)) $(CFLAGS_OTHER)
CFLAGS += -std=c99  -I../library/include -fno-omit-frame-pointer

CFLAGS += $(shell pkg-config --cflags-only-I $(PACKAGES))
LDFLAGS := $(shell pkg-config --libs-only-L --libs-only-other $(PACKAGES))
LDFLAGS += $(patsubst -L%, $(shell pkg-config --variable=ldflag_rpath $(PACKAGES))%, $(shell pkg-config --libs-only-L $(PACKAGES)))
LDLIBS := $(shell pkg-config --libs-only-l $(PACKAGES)) -lm
PYLIB=/usr/lib/python3.8/config-3.8-x86_64-linux-gnu
PYINCL=-I/usr/include -I/usr/include/python3.8
PYLINK=-lpython3.8 -L$(PYLIB)

include $(PETSC_DIR)/$(PETSC_ARCH)/lib/petsc/conf/petscvariables
include $(PETSC_DIR)/$(PETSC_ARCH)/lib/petsc/conf/petscrules
#include $(SLEPC_DIR)/lib/slepc/conf/slepc_common
UTILS_DIR=~/datacenter-profiling/utils
CFLAGS += -I$(UTILS_DIR)


svd: svd.c
	$(LINK.c)  -o $@ $^ $(LDLIBS) -lslepc

parsetest: test_parse.c petsc_webserver.c
	$(LINK.c) -o $@ $^ $(LDLIBS)


webserver: petsc_webserver.c petsc_webserver_driver.c
	$(LINK.c) -o $@ $^ $(LDLIBS) -rdynamic


webserver_launcher: webserver_launcher.c
	$(LINK.c) -o $@ $^ $(PYINCL) $(LDLIBS) $(PYLINK)
