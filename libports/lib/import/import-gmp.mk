REP_INC_DIR += include/gmp

ifeq ($(filter-out $(SPECS),x86),)
  ifeq ($(filter-out $(SPECS),32bit),)
    REP_INC_DIR += include/gmp/x86_32
  endif # 32bit

  ifeq ($(filter-out $(SPECS),64bit),)
    REP_INC_DIR += include/gmp/x86_64
  endif # 32bit

else
REQUIRES += x86
endif # x86
