#
# This is a convenience helper for porting libgcrypt. It downloads and builds
# libgcrypt and libgpg-error within the current working directory and thereby
# makes generated files like config.h readily available.
#

default: install_libgcrypt

PWD := $(shell pwd)

LIBGRYPT     := libgcrypt-1.8.2
LIBGPG_ERROR := libgpg-error-1.27

INSTALL_DIR := $(PWD)/install

URL(${LIBGRYPT})     := https://www.gnupg.org/ftp/gcrypt/libgcrypt/$(LIBGRYPT).tar.bz2
URL(${LIBGPG_ERROR}) := https://www.gnupg.org/ftp/gcrypt/libgpg-error/$(LIBGPG_ERROR).tar.bz2

install/include/gpg-error.h: build/$(LIBGPG_ERROR)/Makefile
	cd build/$(LIBGPG_ERROR); $(MAKE) install

install_libgcrypt: build/$(LIBGRYPT)/Makefile
	cd build/$(LIBGRYPT); $(MAKE) install

build/$(LIBGRYPT)/Makefile : src/$(LIBGRYPT) build/$(LIBGRYPT) install/include/gpg-error.h
	cd build/$(LIBGRYPT); \
		$(PWD)/src/$(LIBGRYPT)/configure --prefix=$(INSTALL_DIR) \
		                                 --with-libgpg-error-prefix=$(INSTALL_DIR)

build/$(LIBGPG_ERROR)/Makefile : src/$(LIBGPG_ERROR) build/$(LIBGPG_ERROR)
	cd build/$(LIBGPG_ERROR); \
		$(PWD)/src/$(LIBGPG_ERROR)/configure --prefix=$(INSTALL_DIR)

build/%:
	mkdir -p $@

src/% : %.tar.bz2
	mkdir -p src
	cd src; tar xf $(PWD)/$<

%.tar.bz2:
	wget ${URL($*)}

clean:
	rm -rf install build src

cleanall: clean
	rm *.tar.bz2
