#
# evaluate bbl_dir immediately, otherwise it won't recognize
# missing ports when checking library dependencies
#

REP_INC_DIR += src/bootstrap/spec/riscv

INC_DIR += $(call select_from_ports,bbl)/src/lib/bbl

SRC_CC  += bootstrap/spec/riscv/platform.cc
SRC_CC  += lib/base/riscv/kernel/interface.cc
SRC_CC  += spec/64bit/memory_map.cc
SRC_S   += bootstrap/spec/riscv/crt0.s

vpath spec/64bit/memory_map.cc $(call select_from_repositories,src/lib/hw)

include $(call select_from_repositories,lib/mk/bootstrap-hw.inc)
