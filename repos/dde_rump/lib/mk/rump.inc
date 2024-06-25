include $(REP_DIR)/lib/mk/rump_common.inc

SHARED_LIB = yes
LIBS      += rump_include

CC_OPT += -DLIBRUMPUSER
SRC_CC  = dummies.cc hypercall.cc bootstrap.cc io.cc sync.cc env.cc

CC_C_OPT += -DHAVE_PROP_DICTIONARY_T
SRC_C     = __main.c \

SRC_NOLINK += accessors.c \
              bswap16.c \
              bswap32.c \
              cdbr.c \
              clock_subr.c \
              cons.c \
              consttime_memequal.c \
              cprng_fast.c \
              cpuset.c \
              crc32.c \
              devsw.c \
              emul.c \
              etfs_wrap.c \
              explicit_memset.c \
              ffs.c \
              getfstypename.c \
              heapsort.c \
              hyperentropy.c \
              imax.c \
              imin.c \
              inet_addr.c \
              init_sysctl_base.c \
              intoa.c \
              intr.c \
              kern_assert.c \
              kern_auth.c \
              kern_cfglock.c \
              kern_descrip.c \
              kern_event.c \
              kern_hook.c \
              kern_ksyms.c \
              kern_ktrace.c \
              kern_malloc.c \
              kern_module.c \
              kern_mutex_obj.c \
              kern_ntptime.c \
              kern_proc.c \
              kern_prot.c \
              kern_rate.c \
              kern_resource.c \
              kern_rndpool.c \
              kern_rndq.c \
              kern_rndsink.c \
              kern_select_50.c \
              kern_stub.c \
              kern_syscall.c \
              kern_sysctl.c \
              kern_tc.c \
              kern_time_50.c \
              kern_time.c \
              kern_timeout.c \
              kern_uidinfo.c \
              kern_xxx.c \
              klock.c \
              kobj_machdep.c \
              kobj_rename.c \
              lmax.c \
              lmin.c \
              locks_up.c \
              ltsleep.c \
              lwproc.c \
              max.c \
              mcount.c \
              md4c.c \
              md5c.c \
              memchr.c \
              memcmp.c \
              memcpy.c \
              memmove.c \
              memset.c \
              mertwist.c \
              min.c \
              mi_vector_hash.c \
              murmurhash.c \
              nist_ctr_drbg.c \
              param.c \
              pmatch.c \
              popcount32.c \
              popcount64.c \
              ppath_extant.c \
              ppath_kmem_alloc.c \
              ppath.c \
              proc_compare.c \
              prop_array.c \
              prop_array_util.c \
              prop_bool.c \
              prop_data.c \
              prop_dictionary.c \
              prop_dictionary_util.c \
              prop_ingest.c \
              prop_kern.c \
              prop_number.c \
              prop_object.c \
              prop_stack.c \
              prop_string.c \
              ptree.c \
              random.c \
              rb.c \
              rijndael-alg-fst.c \
              rijndael-api-fst.c \
              rijndael.c \
              rmd160.c \
              rngtest.c \
              rumpcopy.c \
              rumpkern_if_wrappers.c \
              rump.c \
              rumpkern_syscalls.c \
              rump_syscalls.c \
              scanc.c \
              scheduler.c \
              secmodel.c \
              secmodel_suser.c \
              sha1.c \
              sha2.c \
              signals.c \
              skpc.c \
              sleepq.c \
              snprintb.c \
              strcasecmp.c \
              strcat.c \
              strchr.c \
              strcmp.c \
              strcpy.c \
              strcspn.c \
              strlcat.c \
              strlcpy.c \
              strlen.c \
              strncasecmp.c \
              strncat.c \
              strncmp.c \
              strncpy.c \
              strnlen.c \
              strpbrk.c \
              strrchr.c \
              strsep.c \
              strspn.c \
              strstr.c \
              strtoimax.c \
              strtoll.c \
              strtoull.c \
              strtoul.c \
              strtoumax.c \
              subr_callback.c \
              subr_copy.c \
              subr_cprng.c \
              subr_device.c \
              subr_devsw.c \
              subr_evcnt.c \
              subr_extent.c \
              subr_hash.c \
              subr_humanize.c \
              subr_iostat.c \
              subr_kcpuset.c \
              subr_kmem.c \
              subr_kobj.c \
              subr_log.c \
              subr_lwp_specificdata.c \
              subr_once.c \
              subr_pcq.c \
              subr_percpu.c \
              subr_pool.c \
              subr_prf.c \
              subr_pserialize.c \
              subr_specificdata.c \
              subr_time.c \
              subr_vmem.c \
              subr_workqueue.c \
              subr_xcall.c \
              syscalls.c \
              sys_descrip.c \
              sys_generic.c \
              sys_module.c \
              sys_pipe.c \
              sys_select.c \
              threads.c \
              uipc_sem.c \
              ulmax.c \
              ulmin.c \
              uvm_aobj.c \
              uvm_object.c \
              uvm_readahead.c \
              uvm_swapstub.c \
              vers.c \
              vm.c \
              vnode_if.c \
              xlat_mbr_fstype.c

# prevent the generation of a 'memset()' call in 'memset()'
CC_OPT_memset = -fno-tree-loop-distribute-patterns

INC_DIR += $(REP_DIR)/src/include \
           $(RUMP_BASE)/include \
           $(RUMP_PORT_DIR)/src/lib/libc/include \
           $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern/opt \
           $(RUMP_PORT_DIR)/src/sys \
           $(RUMP_PORT_DIR)/src/sys/sys \
           $(RUMP_PORT_DIR)/src/sys/rump/include \
           $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpdev \
           $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern \
           $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpnet \
           $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpvfs \
           $(RUMP_PORT_DIR)/src/include \
           $(RUMP_PORT_DIR)/src/common/include \
           $(RUMP_PORT_DIR)/common/lib/libc/gen


#
# generic
#
vers.c:
	$(VERBOSE_MK)cd $(RUMP_BASE) && /bin/sh $(RUMP_PORT_DIR)/src/sys/conf/newvers.sh -r -iRUMP_ROAST -n

# trigger build
$(SRC_O): vers.c

#
# rmpns_ prefix rules
#
RUMP_LIB_BASE = $(RUMP_BASE)
include $(REP_DIR)/lib/mk/rump_prefix.inc


vpath %.cc $(REP_DIR)/src/lib/rump

vpath %.c $(RUMP_PORT_DIR)/src/sys/rump/librump/rumpkern
vpath %.c $(RUMP_PORT_DIR)/src/sys/conf
vpath %.c $(RUMP_PORT_DIR)/src/sys/crypto/cprng_fast
vpath %.c $(RUMP_PORT_DIR)/src/sys/crypto/nist_ctr_drbg
vpath %.c $(RUMP_PORT_DIR)/src/sys/crypto/rijndael
vpath %.c $(RUMP_PORT_DIR)/src/sys/compat/common
vpath %.c $(RUMP_PORT_DIR)/src/sys/dev
vpath %.c $(RUMP_PORT_DIR)/src/sys/lib/libkern
vpath %.c $(RUMP_PORT_DIR)/src/sys/kern
vpath %.c $(RUMP_PORT_DIR)/src/sys/secmodel
vpath %.c $(RUMP_PORT_DIR)/src/sys/secmodel/suser
vpath %.c $(RUMP_PORT_DIR)/src/sys/uvm
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/cdb
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/gen
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/gmon
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/hash/murmurhash
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/hash/rmd160
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/hash/sha1
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/hash/sha2
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/inet
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/md
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/stdlib
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/string
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libc/sys
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libppath
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libprop
vpath %.c $(RUMP_PORT_DIR)/src/common/lib/libutil

# vi:set ft=make :

