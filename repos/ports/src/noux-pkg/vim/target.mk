CONFIGURE_ARGS = --disable-selinux --disable-xsmp --disable-xsmp-interact \
                 --disable-netbeans --disable-gtktest --disable-largefile \
                 --disable-acl --disable-gpm --disable-sysmouse \
                 --disable-nls

#
# configure: error: cross-compiling: please set 'vim_cv_toupper_broken'
#
CONFIGURE_ARGS += vim_cv_toupper_broken=yes

#
# configure: error: NOT FOUND!
#       You need to install a terminal library; for example ncurses.
#       Or specify the name of the library with --with-tlib.
#
CONFIGURE_ARGS += --with-tlib=ncurses

#
# configure: error: cross-compiling: please set ...
#
CONFIGURE_ARGS += vim_cv_terminfo=linux
CONFIGURE_ARGS += vim_cv_tty_group=world
CONFIGURE_ARGS += vim_cv_tty_mode=0620
CONFIGURE_ARGS += vim_cv_getcwd_broken=no
CONFIGURE_ARGS += vim_cv_stat_ignores_slash=no
CONFIGURE_ARGS += vim_cv_memmove_handles_overlap=yes

INSTALL_TARGET = install

LIBS += ncurses

include $(REP_DIR)/mk/noux.mk

env.sh: mirror_vim_src.tag flush_config_cache.tag

mirror_vim_src.tag:
	$(VERBOSE)cp -af $(PKG_DIR)/src $(PWD)
	$(VERBOSE)cp -af $(PKG_DIR)/Makefile $(PWD)
	$(VERBOSE)ln -sf $(PKG_DIR)/Filelist $(PWD)
	$(VERBOSE)ln -sf $(PKG_DIR)/runtime $(PWD)
	# let the configure script update the Makefile time stamp
	$(VERBOSE)sed -i "/exit/s/^/touch ..\/Makefile\n/" src/configure
	@touch $@

flush_config_cache.tag:
	$(VERBOSE)rm -f $(PWD)/src/auto/config.cache
	@touch $@

#
# Make the ncurses linking test succeed
#
Makefile: dummy_libs

LDFLAGS += -L$(PWD)

.SECONDARY: dummy_libs
dummy_libs: libncurses.a

libncurses.a:
	$(VERBOSE)$(AR) -rc $@

