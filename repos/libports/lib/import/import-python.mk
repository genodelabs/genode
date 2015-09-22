PYTHON       = python-2.6.4
REP_INC_DIR += include/python
INC_DIR     += $(call select_from_ports,python)/include

ifeq ($(filter-out $(SPECS),x86),)
   ifeq ($(filter-out $(SPECS),32bit),)
     REP_INC_DIR += include/python/spec/x86_32
   endif # 32bit
 
   ifeq ($(filter-out $(SPECS),64bit),)
     REP_INC_DIR += include/python/spec/x86_64
   endif # 64bit
endif # x86

