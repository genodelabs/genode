LIBC := libc-8.2.0

#
# Interface to top-level prepare Makefile
#
PORTS += $(LIBC)

#
# Check for tools
#
$(call check_tool,svn)
$(call check_tool,lex)
$(call check_tool,bison)
$(call check_tool,rpcgen)

#
# Subdirectories to check out from FreeBSD's Subversion repository
#
LIBC_SVN_BASE = http://svn.freebsd.org/base/release/8.2.0

LIBC_CONTRIB_SUB_DIRS = libc libutil include sys_sys sys_netinet sys_netinet6 \
                        sys_net sys_bsm sys_rpc sys_vm sys_arm sys_i386 sys_amd64 \
                        msun gdtoa

LIBC_SVN_libc         = lib/libc
LIBC_SVN_libutil      = lib/libutil
LIBC_SVN_include      = include
LIBC_SVN_sys_sys      = sys/sys
LIBC_SVN_sys_rpc      = sys/rpc
LIBC_SVN_sys_net      = sys/net
LIBC_SVN_sys_netinet  = sys/netinet
LIBC_SVN_sys_netinet6 = sys/netinet6
LIBC_SVN_sys_bsm      = sys/bsm
LIBC_SVN_sys_vm       = sys/vm
LIBC_SVN_sys_arm      = sys/arm/include
LIBC_SVN_sys_i386     = sys/i386/include
LIBC_SVN_sys_amd64    = sys/amd64/include
LIBC_SVN_msun         = lib/msun
LIBC_SVN_gdtoa        = contrib/gdtoa

LIBC_DIRS_TO_CHECKOUT = $(addprefix $(CONTRIB_DIR)/$(LIBC)/,$(LIBC_CONTRIB_SUB_DIRS))

$(LIBC_DIRS_TO_CHECKOUT):
	$(ECHO) "checking out '$(LIBC_SVN_$(notdir $@))' to '$@'"
	$(VERBOSE)mkdir -p $(CONTRIB_DIR)/$(LIBC)
	$(VERBOSE)svn export $(LIBC_SVN_BASE)/$(LIBC_SVN_$(notdir $@)) $@

checkout-libc: $(LIBC_DIRS_TO_CHECKOUT)

#
# Files coming from the include directory
#
LIBC_IMPORT_INCLUDES =  include/libc/strings.h \
                        include/libc/limits.h \
                        include/libc/string.h \
                        include/libc/ctype.h \
                        include/libc/_ctype.h \
                        include/libc/runetype.h \
                        include/libc/stdlib.h \
                        include/libc/stdio.h \
                        include/libc/signal.h \
                        include/libc/unistd.h \
                        include/libc/wchar.h \
                        include/libc/time.h \
                        include/libc/sysexits.h \
                        include/libc/arpa/inet.h \
                        include/libc/arpa/ftp.h \
                        include/libc/arpa/nameser.h \
                        include/libc/arpa/nameser_compat.h \
                        include/libc/arpa/telnet.h \
                        include/libc/arpa/tftp.h \
                        include/libc/resolv.h \
                        include/libc/wctype.h \
                        include/libc/fcntl.h \
                        include/libc/locale.h \
                        include/libc/langinfo.h \
                        include/libc/regex.h \
                        include/libc/paths.h \
                        include/libc/inttypes.h \
                        include/libc/fstab.h \
                        include/libc/netdb.h \
                        include/libc/ar.h \
                        include/libc/stdint.h \
                        include/libc/ieeefp.h \
                        include/libc/memory.h \
                        include/libc/res_update.h \
                        include/libc/rpc/rpc.h \
                        include/libc/netconfig.h \
                        include/libc/rpc/xdr.h \
                        include/libc/rpc/auth.h \
                        include/libc/rpc/clnt_stat.h \
                        include/libc/rpc/clnt.h \
                        include/libc/rpc/clnt_soc.h \
                        include/libc/rpc/rpc_msg.h \
                        include/libc/rpc/auth_unix.h \
                        include/libc/rpc/auth_des.h \
                        include/libc/rpc/svc.h \
                        include/libc/rpc/svc_soc.h \
                        include/libc/rpc/svc_auth.h \
                        include/libc/rpc/pmap_clnt.h \
                        include/libc/rpc/pmap_prot.h \
                        include/libc/rpc/rpcb_clnt.h \
                        include/libc/rpc/rpcb_prot.h \
                        include/libc/rpc/rpcent.h \
                        include/libc/rpcsvc/yp_prot.h \
                        include/libc/rpcsvc/ypclnt.h \
                        include/libc/rpc/des_crypt.h \
                        include/libc/rpc/des.h \
                        include/libc/rpcsvc/nis.h \
                        include/libc/rpcsvc/nis_tags.h \
                        include/libc/rpcsvc/nislib.h \
                        include/libc/rpc/rpc_com.h \
                        include/libc/ifaddrs.h \
                        include/libc/rpc/nettype.h \
                        include/libc/rpc/rpcsec_gss.h \
                        include/libc/gssapi/gssapi.h \
                        include/libc/rpc/raw.h \
                        include/libc/rpcsvc/crypt.h

#
# Files from include directory needed for stdlib
#
# We have to make sure to shadow all gcc headers to avoid conflicts.
#
LIBC_IMPORT_INCLUDES += include/libc/pthread.h \
                        include/libc/sched.h \
                        include/libc/err.h \
                        include/libc/getopt.h \
                        include/libc/search.h \
                        include/libc/ktrace.h \
                        include/libc/varargs.h \
                        include/libc/stddef.h \
                        include/libc/stdbool.h \
                        include/libc/assert.h \
                        include/libc/monetary.h \
                        include/libc/printf.h \
                        include/libc/math.h

#
# Files from include directory needed for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc/vis.h \
                        include/libc/libgen.h \
                        include/libc/dirent.h \
                        include/libc/dlfcn.h \
                        include/libc/link.h \
                        include/libc/fmtmsg.h \
                        include/libc/fnmatch.h \
                        include/libc/fts.h \
                        include/libc/ftw.h \
                        include/libc/db.h \
                        include/libc/grp.h \
                        include/libc/nsswitch.h \
                        include/libc/pthread_np.h \
                        include/libc/syslog.h \
                        include/libc/pwd.h \
                        include/libc/utmp.h \
                        include/libc/ttyent.h \
                        include/libc/stringlist.h \
                        include/libc/glob.h \
                        include/libc/termios.h \
                        include/libc/a.out.h \
                        include/libc/elf-hints.h \
                        include/libc/nlist.h \
                        include/libc/spawn.h \
                        include/libc/readpassphrase.h \
                        include/libc/semaphore.h \
                        include/libc/_semaphore.h \
                        include/libc/setjmp.h \
                        include/libc/elf.h \
                        include/libc/ulimit.h \
                        include/libc/utime.h \
                        include/libc/wordexp.h

#
# Files from sys/vm needed for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc/vm/vm_param.h \
                        include/libc/vm/vm.h \
                        include/libc/vm/pmap.h

#
# Files coming from the sys/net directories
#
LIBC_IMPORT_INCLUDES += include/libc/net/if.h \
                        include/libc/net/if_dl.h \
                        include/libc/net/if_types.h

#
# Files coming from the sys/netinet and sys/netinet6 directories
#
LIBC_IMPORT_INCLUDES += include/libc/netinet/in.h \
                        include/libc/netinet/in_systm.h \
                        include/libc/netinet6/in6.h \
                        include/libc/netinet/ip.h \
                        include/libc/netinet/tcp.h

#
# Files coming from the sys/rpc directory
#
LIBC_IMPORT_INCLUDES += include/libc/sys/rpc/types.h

#
# Files coming from the sys/sys directory
#
LIBC_IMPORT_INCLUDES += include/libc/sys/_types.h \
                        include/libc/sys/limits.h \
                        include/libc/sys/cdefs.h \
                        include/libc/sys/_null.h \
                        include/libc/sys/types.h \
                        include/libc/sys/_pthreadtypes.h \
                        include/libc/sys/syslimits.h \
                        include/libc/sys/select.h \
                        include/libc/sys/_sigset.h \
                        include/libc/sys/_timeval.h \
                        include/libc/sys/timespec.h \
                        include/libc/sys/_timespec.h \
                        include/libc/sys/stat.h \
                        include/libc/sys/signal.h \
                        include/libc/sys/unistd.h \
                        include/libc/sys/time.h \
                        include/libc/sys/param.h \
                        include/libc/sys/stdint.h \
                        include/libc/sys/event.h \
                        include/libc/errno.h \
                        include/libc/sys/errno.h \
                        include/libc/sys/poll.h

#
# Files from sys/sys needed for stdlib and stdio and gen lib
#
LIBC_IMPORT_INCLUDES += include/libc/sys/queue.h \
                        include/libc/sys/mman.h \
                        include/libc/sys/stddef.h \
                        include/libc/sys/sysctl.h \
                        include/libc/sys/uio.h \
                        include/libc/sys/_iovec.h \
                        include/libc/sys/ktrace.h \
                        include/libc/sys/ioctl.h \
                        include/libc/sys/ttycom.h \
                        include/libc/sys/ioccom.h \
                        include/libc/sys/filio.h \
                        include/libc/sys/sockio.h \
                        include/libc/sys/wait.h \
                        include/libc/sys/file.h \
                        include/libc/sys/fcntl.h \
                        include/libc/sys/resource.h \
                        include/libc/sys/disklabel.h \
                        include/libc/sys/link_elf.h \
                        include/libc/sys/endian.h \
                        include/libc/sys/mount.h \
                        include/libc/sys/ucred.h \
                        include/libc/sys/dirent.h \
                        include/libc/sys/cpuset.h \
                        include/libc/sys/socket.h \
                        include/libc/sys/un.h \
                        include/libc/sys/ttydefaults.h \
                        include/libc/sys/imgact_aout.h \
                        include/libc/sys/elf32.h \
                        include/libc/sys/elf64.h \
                        include/libc/sys/elf_generic.h \
                        include/libc/sys/elf_common.h \
                        include/libc/sys/nlist_aout.h \
                        include/libc/sys/ipc.h \
                        include/libc/sys/sem.h \
                        include/libc/sys/exec.h \
                        include/libc/sys/_lock.h \
                        include/libc/sys/_mutex.h \
                        include/libc/sys/statvfs.h \
                        include/libc/sys/ucontext.h \
                        include/libc/sys/syslog.h \
                        include/libc/sys/times.h \
                        include/libc/sys/utsname.h \
                        include/libc/sys/elf.h \
                        include/libc/sys/mtio.h

#
# Files coming from the sys/arm/include directory
#
LIBC_IMPORT_INCLUDES += include/libc-arm/machine/_types.h \
                        include/libc-arm/machine/endian.h \
                        include/libc-arm/machine/_limits.h \
                        include/libc-arm/machine/signal.h \
                        include/libc-arm/machine/trap.h \
                        include/libc-arm/machine/_stdint.h \
                        include/libc-arm/machine/pte.h \
                        include/libc-arm/machine/cpuconf.h \
                        include/libc-arm/machine/sysarch.h \
                        include/libc-arm/machine/armreg.h \
                        include/libc-arm/machine/ieee.h \
                        include/libc-arm/machine/frame.h \
                        include/libc-arm/machine/sigframe.h \
                        include/libc-arm/machine/vm.h \
                        include/libc-arm/stdarg.h \
                        include/libc-arm/float.h


#
# Files coming from the sys/i386/include directory
#
LIBC_IMPORT_INCLUDES += include/libc-i386/machine/_types.h \
                        include/libc-i386/machine/endian.h \
                        include/libc-i386/machine/_limits.h \
                        include/libc-i386/machine/signal.h \
                        include/libc-i386/machine/trap.h \
                        include/libc-i386/machine/_inttypes.h \
                        include/libc-i386/machine/_stdint.h \
                        include/libc-i386/machine/param.h \
                        include/libc-i386/machine/vm.h \
                        include/libc-i386/machine/specialreg.h \
                        include/libc-i386/stdarg.h \
                        include/libc-i386/float.h

#
# Files coming from the sys/amd64/include directory
#
LIBC_IMPORT_INCLUDES += include/libc-amd64/machine/_types.h \
                        include/libc-amd64/machine/endian.h \
                        include/libc-amd64/machine/_limits.h \
                        include/libc-amd64/machine/signal.h \
                        include/libc-amd64/machine/trap.h \
                        include/libc-amd64/machine/_inttypes.h \
                        include/libc-amd64/machine/_stdint.h \
                        include/libc-amd64/machine/param.h \
                        include/libc-amd64/machine/vm.h \
                        include/libc-amd64/machine/specialreg.h \
                        include/libc-amd64/stdarg.h \
                        include/libc-amd64/float.h

#
# Files from sys/arm/include needed for stdlib and stdio
#
LIBC_IMPORT_INCLUDES += include/libc-arm/machine/cpufunc.h \
                        include/libc-arm/machine/vmparam.h \
                        include/libc-arm/machine/atomic.h \
                        include/libc-arm/arith.h \
                        include/libc-arm/_fpmath.h \

#
# Files from sys/i386/include needed for stdlib and stdio
#
LIBC_IMPORT_INCLUDES += include/libc-i386/machine/cpufunc.h \
                        include/libc-i386/machine/vmparam.h \
                        include/libc-i386/machine/atomic.h \
                        include/libc-i386/arith.h \
                        include/libc-i386/_fpmath.h \

#
# Files from sys/amd64/include needed for stdlib and stdio
#
LIBC_IMPORT_INCLUDES += include/libc-amd64/machine/cpufunc.h \
                        include/libc-amd64/machine/vmparam.h \
                        include/libc-amd64/machine/atomic.h \
                        include/libc-amd64/arith.h \
                        include/libc-amd64/_fpmath.h \

#
# Files from sys/arm/include needed for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc-arm/machine/elf.h \
                        include/libc-arm/machine/exec.h \
                        include/libc-arm/machine/reloc.h \
                        include/libc-arm/machine/pmap.h \
                        include/libc-arm/machine/ucontext.h \
                        include/libc-arm/machine/setjmp.h \
                        include/libc-arm/machine/asm.h \
                        include/libc-arm/machine/param.h \
                        include/libc-arm/machine/_inttypes.h \
                        include/libc-arm/machine/ieeefp.h \
                        include/libc-arm/SYS.h

#
# Files from sys/i386/include needed for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc-i386/machine/elf.h \
                        include/libc-i386/machine/exec.h \
                        include/libc-i386/machine/reloc.h \
                        include/libc-i386/machine/pmap.h \
                        include/libc-i386/machine/ucontext.h \
                        include/libc-i386/machine/setjmp.h \
                        include/libc-i386/machine/asm.h \
                        include/libc-i386/machine/ieeefp.h \
                        include/libc-i386/SYS.h

#
# Files from sys/amd64/include needed for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc-amd64/machine/elf.h \
                        include/libc-amd64/machine/exec.h \
                        include/libc-amd64/machine/reloc.h \
                        include/libc-amd64/machine/pmap.h \
                        include/libc-amd64/machine/ucontext.h \
                        include/libc-amd64/machine/setjmp.h \
                        include/libc-amd64/machine/asm.h \
                        include/libc-amd64/machine/ieeefp.h \
                        include/libc-amd64/SYS.h

#
# Files needed for math lib
#
LIBC_IMPORT_INCLUDES += include/libc/complex.h

#
# Files from libc/arm needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-arm/gd_qnan.h

#
# Files from libc/i386 needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-i386/gd_qnan.h

#
# Files from libc/amd64 needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-amd64/gd_qnan.h

#
# Files from msun/arm needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-arm/fenv.h

#
# Files from msun/i387 needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-i386/fenv.h

#
# Files from msun/amd64 needed for gdtoa lib
#
LIBC_IMPORT_INCLUDES += include/libc-amd64/fenv.h

#
# Files from sys/bsm for gen lib
#
LIBC_IMPORT_INCLUDES += include/libc/bsm/audit.h

#
# Generate files needed for compiling libc-net
#
libc_gen_nslexer: $(CONTRIB_DIR)/$(LIBC)/libc/net/nslexer.l
	$(VERBOSE)$(LEX) -P_nsyy -t $< | \
		sed -e '/YY_BUF_SIZE/s/16384/1024/' \
		> $(CONTRIB_DIR)/$(LIBC)/libc/net/nslexer.c

libc_gen_nsparser: $(CONTRIB_DIR)/$(LIBC)/libc/net/nsparser.y
	$(VERBOSE)bison -d -p_nsyy $< \
		--defines=$(CONTRIB_DIR)/$(LIBC)/libc/net/nsparser.h \
		--output=$(CONTRIB_DIR)/$(LIBC)/libc/net/nsparser.c

libc_net_generate: libc_gen_nslexer libc_gen_nsparser

#
# Generate files needed for compiling libc-rpc
#
RPCB_FILES     = rpcb_prot.x
SRC_RPCB_FILES = $(addprefix $(CONTRIB_DIR)/$(LIBC)/include/rpc/,$(RPCB_FILES))
GEN_RPCB_FILES = $(SRC_RPCB_FILES:.x=.h)

#
# Unfortunatly include/rpcsvc contains a lot of .x files and to resolve their
# dependencies would by cumbersome. So we include all of them instead of only
# the ones we currently need.
#
RPCSVC_FILES    = bootparam_prot.x nfs_prot.x nlm_prot.x rstat.x ypupdate_prot.x \
                  crypt.x nis_cache.x pmap_prot.x rwall.x yp.x \
                  key_prot.x nis_callback.x rex.x sm_inter.x ypxfrd.x \
                  klm_prot.x nis_object.x rnusers.x spray.x \
                  mount.x nis.x rquota.x yppasswd.x

SRC_RPCSVC_FILES = $(addprefix $(CONTRIB_DIR)/$(LIBC)/include/rpcsvc/,$(RPCSVC_FILES))
GEN_RPCSVC_FILES = $(SRC_RPCSVC_FILES:.x=.h)

# nis_object.h is needed by nis.h so we have to generate this header first
$(CONTRIB_DIR)/$(LIBC)/include/rpcsvc/nis.h: $(CONTRIB_DIR)/$(LIBC)/include/rpcsvc/nis_object.x

libc_rpc_generate:
	$(VERBOSE)for header in $(GEN_RPCB_FILES); do\
		if [ ! -e "$$header" ]; then \
			rpcgen -C -h -DWANT_NFS3 $${header%.h}.x -o $$header; \
		fi; done
	$(VERBOSE)for header in $(GEN_RPCSVC_FILES); do\
		if [ ! -e "$$header" ]; then \
			rpcgen -C -h -DWANT_NFS3 $${header%.h}.x -o $$header; \
		fi; done

##
# Shortcut for creating a symlink
#
# \param $(1) prefix prepended to symlink origin, used for creating relative
#             symlinks
#
libc_gen_symlink_subsub       = $(VERBOSE)mkdir -p $(dir $@); ln -sf       ../../$< $@
libc_gen_symlink_subsubsub    = $(VERBOSE)mkdir -p $(dir $@); ln -sf    ../../../$< $@
libc_gen_symlink_subsubsubsub = $(VERBOSE)mkdir -p $(dir $@); ln -sf ../../../../$< $@

include/libc/arpa/%.h: $(CONTRIB_DIR)/$(LIBC)/include/arpa/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/gssapi/%.h: $(CONTRIB_DIR)/$(LIBC)/include/gssapi/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/net/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_net/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/netinet/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_netinet/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/netinet6/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_netinet6/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/rpc/%.h: $(CONTRIB_DIR)/$(LIBC)/include/rpc/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/rpcsvc/%.h: $(CONTRIB_DIR)/$(LIBC)/include/rpcsvc/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/%.h: $(CONTRIB_DIR)/$(LIBC)/include/%.h
	$(libc_gen_symlink_subsub)

include/libc/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_sys/%.h
	$(libc_gen_symlink_subsub)

include/libc/%.h: $(CONTRIB_DIR)/$(LIBC)/msun/src/%.h
	$(libc_gen_symlink_subsub)

include/libc-arm/%.h: $(CONTRIB_DIR)/$(LIBC)/msun/arm/%.h
	$(libc_gen_symlink_subsub)

include/libc-i386/%.h: $(CONTRIB_DIR)/$(LIBC)/msun/i387/%.h
	$(libc_gen_symlink_subsub)

include/libc-amd64/%.h: $(CONTRIB_DIR)/$(LIBC)/msun/amd64/%.h
	$(libc_gen_symlink_subsub)

include/libc/sys/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_sys/%.h
	$(libc_gen_symlink_subsubsub)

include/libc/sys/rpc/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_rpc/%.h
	$(libc_gen_symlink_subsubsubsub)

include/libc-arm/machine/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_arm/%.h
	$(libc_gen_symlink_subsubsub)

include/libc-i386/machine/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_i386/%.h
	$(libc_gen_symlink_subsubsub)

include/libc-amd64/machine/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_amd64/%.h
	$(libc_gen_symlink_subsubsub)

include/libc-arm/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_arm/%.h
	$(libc_gen_symlink_subsub)

include/libc-i386/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_i386/%.h
	$(libc_gen_symlink_subsub)

include/libc-amd64/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_amd64/%.h
	$(libc_gen_symlink_subsub)

include/libc-arm/%.h: $(CONTRIB_DIR)/$(LIBC)/libc/arm/%.h
	$(libc_gen_symlink_subsub)

include/libc-i386/%.h: $(CONTRIB_DIR)/$(LIBC)/libc/i386/%.h
	$(libc_gen_symlink_subsub)

include/libc-amd64/%.h: $(CONTRIB_DIR)/$(LIBC)/libc/amd64/%.h
	$(libc_gen_symlink_subsub)

include/libc/bsm/audit.h: $(CONTRIB_DIR)/$(LIBC)/sys_bsm/audit.h
	$(libc_gen_symlink_subsubsub)

include/libc/vm/%.h: $(CONTRIB_DIR)/$(LIBC)/sys_vm/%.h
	$(libc_gen_symlink_subsubsub)

apply_patches-libc: checkout-libc
	$(VERBOSE)find ./src/lib/libc/patches/ -name "*.patch" |\
		xargs -ixxx sh -c "patch -p0 -r - -N -d $(CONTRIB_DIR)/$(LIBC) < xxx" || true

#
# Use new make instance for symlink creation. Otherwise the implicit rules
# above do not work as expected (because the dependent names do not exist
# at the invokation time of the original make instance and are created
# as side effect of the 'LIBC_DIRS_TO_CHECKOUT' out rule).
#
create_include_symlinks-libc: checkout-libc
	$(VERBOSE)make $(LIBC_IMPORT_INCLUDES) VERBOSE=$(VERBOSE)

prepare-libc: apply_patches-libc libc_net_generate libc_rpc_generate create_include_symlinks-libc

clean_include_symlinks-libc:
	$(VERBOSE)find include -type l -delete

clean_include_subdirs-libc: clean_include_symlinks-libc
	$(VERBOSE)find include -type d -empty -delete

clean-libc: clean_include_subdirs-libc
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBC)

