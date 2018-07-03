PYTHON       = python-3.6.5
REP_INC_DIR += include/python3
INC_DIR     += $(call select_from_ports,python3)/include

ifeq ($(filter-out $(SPECS),x86),)
   ifeq ($(filter-out $(SPECS),32bit),)
     REP_INC_DIR += include/python3/spec/x86_32
   endif # 32bit
 
   ifeq ($(filter-out $(SPECS),64bit),)
     REP_INC_DIR += include/python3/spec/x86_64
   endif # 64bit
endif # x86

