REP_INC_DIR += include/gmp

ifeq ($(filter-out $(SPECS),arm),)
	REP_INC_DIR += include/gmp/32bit
	REP_INC_DIR += include/gmp/arm
endif

ifeq ($(filter-out $(SPECS),x86_32),)
	REP_INC_DIR += include/gmp/32bit
	REP_INC_DIR += include/gmp/x86_32
endif

ifeq ($(filter-out $(SPECS),x86_64),)
	REP_INC_DIR += include/gmp/64bit
	REP_INC_DIR += include/gmp/x86_64
endif
