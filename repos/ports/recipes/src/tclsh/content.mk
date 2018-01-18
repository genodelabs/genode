content: src/noux-pkg/tcl src/noux-pkg/tclsh LICENSE README

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/tcl)

TCL_CONTENT := unix generic compat libtommath

src/noux-pkg/tcl:
	mkdir -p $@/library
	cp -a $(addprefix $(PORT_DIR)/src/noux-pkg/tcl/,$(TCL_CONTENT)) $@
	cp -a $(PORT_DIR)/src/noux-pkg/tcl/library/init.tcl $@/library/

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

