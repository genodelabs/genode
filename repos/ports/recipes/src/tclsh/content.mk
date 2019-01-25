content: src/noux-pkg/tcl src/noux-pkg/tclsh LICENSE README

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/tcl)

TCL_CONTENT := unix generic compat libtommath

PORT_LIB_FILES := \
	clock.tcl dde/pkgIndex.tcl http/pkgIndex.tcl http1.0/pkgIndex.tcl init.tcl \
	msgcat/msgcat.tcl msgcat/pkgIndex.tcl opt/pkgIndex.tcl package.tcl \
	platform/pkgIndex.tcl reg/pkgIndex.tcl tcltest/pkgIndex.tcl tm.tcl \
	tzdata/Etc/UTC tzdata/UTC tclIndex

src/noux-pkg/tcl:
	mkdir -p $@/library
	cp -a $(addprefix $(PORT_DIR)/src/noux-pkg/tcl/,$(TCL_CONTENT)) $@
	$(VERBOSE)tar -C $(PORT_DIR)/src/noux-pkg/tcl/library -cf - $(PORT_LIB_FILES) | tar -C $@/library -xf -

src/noux-pkg/tclsh:
	mkdir -p $@
	cp -a $(REP_DIR)/src/noux-pkg/tclsh/* $@

LICENSE:
	cp $(PORT_DIR)/src/noux-pkg/tcl/license.terms $@

include $(REP_DIR)/ports/tcl.port

README:
	( echo "This archive is a stripped-down version of the Tcl source archive"; \
	  echo "that merely contains the source codes needed to build tclsh for Genode."; \
	  echo "For the original source archive, refer to:"; \
	  echo "${URL(tcl)}" ) > $@

