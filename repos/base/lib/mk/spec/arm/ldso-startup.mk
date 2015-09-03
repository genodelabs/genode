SRC_CC += unwind_exidx.cc

vpath unwind_exidx.cc $(REP_DIR)/src/lib/ldso/startup

include $(call select_from_repositories,lib/mk/ldso-startup.mk)
