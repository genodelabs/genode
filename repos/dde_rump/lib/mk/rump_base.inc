
include $(REP_DIR)/lib/mk/rump.inc
include $(REP_DIR)/lib/import/import-rump.mk

RUMP_OBJ    = $(RUMP_BASE)/obj
RUMP_SRC    = $(RUMP_CONTRIB_DIR)/src
RUMP_TOOLS  = $(RUMP_BASE)/tools
RMAKE       = $(RUMP_TOOLS)/rumpmake

#
# Can be up to 4 for most verbose output
#
VERBOSE_LEVEL ?= 0

#
# Sources
#
SRC_CC = dummies.cc hypercall.cc bootstrap.cc io.cc sync.cc misc.cc env.cc

#
# TARGET to trigger rump build
#
SOURCE = $(addprefix $(REP_DIR)/src/lib/rump/,$(SRC_CC))

#
# Rump build script
#
BUILD_CMD = BUILD_CC=cc BUILD_AR=ar BUILD_NM=nm BUILD_OBJCOPY=objcopy \
            AR=$(AR) NM=$(NM) OBJCOPY=$(OBJCOPY) CC_TARGET=$(shell basename $(CC)) \
            $(RUMP_CONTRIB_DIR)/buildrump.sh -k -V'MAKEVERBOSE=$(VERBOSE_LEVEL)' \
            -V'NOGCCERROR=1' $(RUMP_OPT) -V'RUMP_LOCKS_UP=yes' \
            -s $(RUMP_SRC) -T $(RUMP_TOOLS) -o $(RUMP_OBJ) -d $(RUMP_BASE) \
            -F"DBG=$(CC_OLEVEL) -g"

#
# Linkage
#
ARCHIVE = $(RUMP_LIB)/librump.a
CC_OPT += -DLIBRUMPUSER


INCSDIRS = adosfs altq arpa crypto dev evbarm filecorefs fs i386 isofs miscfs \
           msdosfs net net80211 netatalk netbt netinet netinet6 netipsec \
           netisdn netkey netmpls netnatm netsmb nfs ntfs ppath prop \
           protocols rpc rpcsvc ssp sys ufs uvm x86

#
# Make sure include directory exists
#
$(shell mkdir -p $(RUMP_BASE)/include)

#
# Filter potential ccache call
#
RUMP_CC = $(filter-out ccache, $(CC))

#
# Build nbmake for host platform
#
$(RUMP_TOOLS):
	CC=$(RUMP_CC) $(BUILD_CMD) -N -V RUMP_KERNEL_IS_LIBC=1 tools
	echo 'CPPFLAGS+=-DMAXPHYS=32768' >> $(RUMP_TOOLS)/mk.conf
	echo 'CPPFLAGS+= -fPIC' >> $(RUMP_TOOLS)/mk.conf

#
# Setup install directory
#
$(RUMP_OBJ)/dest.stage: $(RUMP_TOOLS)
	CC=$(RUMP_CC) $(BUILD_CMD) setupdest

#
# Build librump
#
$(ARCHIVE): $(RUMP_OBJ)/dest.stage
	@echo "Building rump library ... $(ARCHIVE)"
	CC=$(RUMP_CC) $(BUILD_CMD) -k build install

#
# Install kernel, libc, and pthread headers
#
$(RUMP_BASE)/include/.prepared: $(ARCHIVE)
	@for i in $(INCSDIRS); do \
		mkdir -p $(RUMP_BASE)/include/$$i; done
	@echo "Installing headers ... (this may take a while)"
	@cd $(RUMP_SRC)/sys; $(RMAKE) -k obj >/dev/null 2>&1
	@cd $(RUMP_SRC)/sys;  $(RMAKE) -k includes >/dev/null 2>&1
	@cd $(RUMP_SRC)/include; $(RMAKE) -k includes > /dev/null 2>&1
	@cd $(RUMP_CONTRIB_DIR)/nblibs/lib/libc; $(RMAKE) includes
	@cd $(RUMP_CONTRIB_DIR)/nblibs/lib/libpthread; $(RMAKE) includes >/dev/null 2>&1
	@touch $(RUMP_BASE)/include/.prepared

#
# Trigger rump build
#
$(SOURCE):  $(RUMP_BASE)/include/.prepared

vpath %.cc $(REP_DIR)/src/lib/rump

# vi:set ft=make :
