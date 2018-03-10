CFLAGS = -Wall
DEBUG = y

VSERSION = $(shell pwd | xargs basename | sed 's/\(.*\)-\(.*\)/\2/g')
SVNVERSION = $(shell export LC_ALL=C; svn info | grep Revision | sed 's/\(.*\):\(.*\)/\2/g' | sed s/[[:space:]]//g)

DESTDIR+=""

ifeq ($(DESTDIR),"")
    DESTDIR=/usr
endif

ifeq ($(DEBUG), y)
   CFLAGS += -g
else
   CFLAGS += -O2
endif

OS_TYPE=$(shell uname -r)
OS_DRIVER_PATH=/lib/modules/$(OS_TYPE)/kernel/drivers/net
OS_SERVICE_PATH=/usr/lib/systemd/system
OS_KERNEL_LOAD_SCRIPT=/etc/sysconfig/modules


ROOT_PATH=$(shell pwd)
export ROOT_PATH

GCC_PATH=$(ROOT_PATH)/gcc
export GCC_PATH

CC=$(GCC_PATH)/bin/gcc
export CC

CXX=$(GCC_PATH)/bin/g++
export CXX

RTE_TARGET = x86_64-native-linuxapp-gcc
export RTE_TARGET

RTE_SDK = $(shell pwd)/dpdk
export RTE_SDK

LIBRARY_PATH = $(ROOT_PATH)/library:$LIBRARY_PATH
export LIBRARY_PATH

ALPHA_GEN=alpha_gen
DPDK=dpdk
ALPHA=alpha
ALAPI=alpha_api
ALAPI_LIB=restbed
OPENSSL=openssl
LIBSTD_TMP= libstdc++.6.0.21

SUBDIRS := $(ALPHA) 

MAKE = make
RM = rm
CP = cp
MKDIR = mkdir
CD = cd
CMAKE = cmake
LS    = ls
PYTHON = python

.PHONY:  all $(DPDK) $(OPENSSL)  $(ALAPI_LIB) $(ALPHA_GEN) $(ALAPI) $(ALPHA) rpm_aware rpm_libgcc rpm

all: $(DPDK)  $(OPENSSL) $(ALAPI_LIB) $(ALAPI) $(ALPHA_GEN) $(ALPHA)


$(DPDK):
	@$(RM)  -rf $(DPDK)/$(RTE_TARGET)
	@$(MAKE) -C $(DPDK) install T=$(RTE_TARGET)

$(ALAPI_LIB):
	@$(CD) $(ROOT_PATH)/$(ALAPI_LIB); rm -rf build; mkdir -p build; cd build;cmake -DBUILD_TESTS=NO -DBUILD_EXAMPLES=NO -DBUILD_SSL=YES -DBUILD_SHARED=YES -DCMAKE_CXX_COMPILER=$(GCC_PATH)/bin/g++ -DCMAKE_INSTALL_PREFIX=$(ROOT_PATH) -Dssl_LIBRARY=$(ROOT_PATH)/include/openssl/lib/libssl.a -Dcrypto_LIBRARY=$(ROOT_PATH)/include/openssl/lib/libcrypto.a -Dssl_INCLUDE=$(ROOT_PATH)/include/openssl/include $(ROOT_PATH)/restbed; make install;cd $(ROOT_PATH) 

$(ALPHA):
	@$(RM) -rf $(ALPHA)/build
	$(CD) $(ROOT_PATH)/auto; python gen_errmsg.py; $(CD) ..
	$(CP) -rf $(ALPHA)/al_to_api.h include/
	$(CP) -rf $(ALPHA)/al_log.h include/
	$(CP) -rf $(ALPHA)/al_common.h include/
	$(CP) -rf $(ALAPI)/al_api.h include/
	$(CP) -rf $(ALAPI)/al_to_main.h include/
	@$(MAKE) -C $(ALPHA)

$(ALPHA_GEN):
	@find $(ALPHA_GEN) -name *.o | xargs rm -f
	@$(MAKE) -C $(ALPHA_GEN)

$(ALAPI): $(ALAPI_LIB)
	@find $(ALAPI) -name *.o | xargs rm -f
	$(CD) $(ROOT_PATH)/auto; python gen_errmsg.py; $(CD) ..
	$(CP) -rf $(ALPHA)/al_to_api.h include/
	$(CP) -rf $(ALPHA)/al_log.h include/
	$(CP) -rf $(ALPHA)/al_common.h include/
	$(CP) -rf $(ALAPI)/al_api.h include/
	$(CP) -rf $(ALAPI)/al_to_main.h include/
	@$(MAKE) -C $(ALAPI)

$(OPENSSL):
	@$(RM) -rf $(ROOT_PATH)/openssl/*
	@$(CD) $(ROOT_PATH)/openssl-src-1.0.2; make clean; ./config shared --prefix=$(ROOT_PATH)/include/openssl --openssldir=$(ROOT_PATH)/include/openssl; make -j4; make install; $(CD) $(ROOT_PATH)

clean:
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean|| exit 1; done
	@$(RM) -rf $(ROOT_PATH)/build
	@$(RM) -rf $(ROOT_PATH)/alpha/build
	@$(RM) -rf $(ROOT_PATH)/$(ALPHA)/build
	@$(RM) -rf $(ROOT_PATH)/include/*
	@$(RM) -rf $(ROOT_PATH)/library/*

install:
	$(MKDIR) -p $(DESTDIR)/usr/bin
	$(MKDIR) -p $(DESTDIR)/usr/sbin
	$(MKDIR) -p $(DESTDIR)/usr/lib64
	$(MKDIR) -p $(DESTDIR)$(OS_DRIVER_PATH)
	$(MKDIR) -p $(DESTDIR)$(OS_SERVICE_PATH)
	$(MKDIR) -p $(DESTDIR)$(OS_KERNEL_LOAD_SCRIPT)
	$(CP) -f $(ROOT_PATH)/$(ALPHA)/build/$(ALPHA) $(DESTDIR)/usr/bin
	$(CP) -f $(ROOT_PATH)/$(DPDK)/$(RTE_TARGET)/kmod/*.ko $(DESTDIR)$(OS_DRIVER_PATH)
	$(CP) -f $(ROOT_PATH)/library/*  $(DESTDIR)/usr/lib64
	$(CP) -f $(ROOT_PATH)/$(ALPHA_GEN)/alpha_gen  $(DESTDIR)/usr/bin
	$(CP) -f $(ROOT_PATH)/script/alpha.sh  $(DESTDIR)/usr/sbin
	$(CP) -f $(ROOT_PATH)/script/alpha_init.sh  $(DESTDIR)/usr/sbin
	$(CP) -f $(ROOT_PATH)/script/alpha_install.sh  $(DESTDIR)/usr/sbin
	$(CP) -f $(ROOT_PATH)/script/alpha.service $(DESTDIR)$(OS_SERVICE_PATH)
	$(CP) -f $(ROOT_PATH)/script/alpha_init.service $(DESTDIR)$(OS_SERVICE_PATH)
	$(CP) -f $(ROOT_PATH)/script/alpha_ko.modules $(DESTDIR)$(OS_KERNEL_LOAD_SCRIPT)

rpm: rpm_aware rpm_libgcc

rpm_aware:
	rm -rf build
	rm -rf library
	rm -rf include
	cd $(ROOT_PATH)/openssl-src-1.0.2; make clean; cd $(ROOT_PATH)
	@sed -i 's/Release:\(.*\)/Release: $(SVNVERSION)/g' aware.spec
	tar -zcf ~/rpmbuild/SOURCES/Aware-$(VSERSION).tar.gz ../Aware-$(VSERSION)  --exclude test --exclude example --exclude .git --exclude x86_64-native-linuxapp-gcc --exclude .svn --exclude ./include  --exclude ./library --exclude alpha/build
	rpmbuild -ba aware.spec

rpm_libgcc:
	-rm -rf $(ROOT_PATH)/$(LIBSTD_TMP)
	mkdir -p ./$(LIBSTD_TMP)
	cp -rf $(ROOT_PATH)/gcc/* $(LIBSTD_TMP)
	tar -zcf ~/rpmbuild/SOURCES/libstdc++.6.0.21.tar.gz ./$(LIBSTD_TMP) --exclude .svn  
	rpmbuild -ba gcc/libstdc++.6.0.21.spec
	-rm -rf ./$(LIBSTD_TMP)

