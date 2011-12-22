#
# Prevent execution of any rule contained in $(TARGET_MK) as default rule
#
all:

#
# Utility for selecting files from the list of repositories
#
select_from_repositories = $(firstword $(foreach REP,$(REPOSITORIES),$(wildcard $(REP)/$(1))))

#
# Include target build instructions to aquire library dependecies
#
PRG_DIR := $(dir $(TARGET_MK))
include $(TARGET_MK)

#
# Include lib-import description files
#
include $(foreach LIB,$(LIBS),$(call select_from_repositories,lib/import/import-$(LIB).mk))

#
# Add globally defined library supplements
#
include $(SPEC_FILES)
LIBS += $(PRG_LIBS)

#
# Determine location of $(TARGET_MK) within 'src/', remove trailing slash
#
PRG_REL_DIR := $(subst $(REP_DIR)/src/,,$(PRG_DIR))
PRG_REL_DIR := $(PRG_REL_DIR:/=)

#
# Prevent generation of program rule if requirements are unsatisfied
#
UNSATISFIED_REQUIREMENTS = $(filter-out $(SPECS),$(REQUIRES))
ifneq ($(UNSATISFIED_REQUIREMENTS),)
all:
	@$(ECHO) "Skip target $(PRG_REL_DIR) because it requires $(DARK_COL)$(UNSATISFIED_REQUIREMENTS)$(DEFAULT_COL)"
else
all: gen_prg_rule
endif

include $(LIB_PROGRESS_LOG)
LIBS_TO_VISIT = $(filter-out $(LIBS_READY),$(LIBS))

#
# Generate program rule
#
gen_prg_rule:
ifneq ($(LIBS),)
	@(echo "DEP_$(TARGET).prg = $(foreach l,$(LIBS),$l.lib \$$(DEP_$l.lib))"; \
	  echo "") >> $(LIB_DEP_FILE)
endif
	@(echo "$(TARGET).prg: $(addsuffix .lib,$(LIBS))"; \
	  echo "	@\$$(MKDIR) -p $(PRG_REL_DIR)"; \
	  echo "	\$$(VERBOSE_MK)\$$(MAKE) $(VERBOSE_DIR) -C $(PRG_REL_DIR) -f \$$(BASE_DIR)/mk/prg.mk \\"; \
	  echo "	     REP_DIR=$(REP_DIR) \\"; \
	  echo "	     PRG_REL_DIR=$(PRG_REL_DIR) \\"; \
	  echo "	     BUILD_BASE_DIR=$(BUILD_BASE_DIR) \\"; \
	  echo "	     DEPS=\"\$$(DEP_$(TARGET).prg)\" \\"; \
	  echo "	     SHELL=$(SHELL) \\"; \
	  echo "	     INSTALL_DIR=\"\$$(INSTALL_DIR)\""; \
	  echo "") >> $(LIB_DEP_FILE)
	@for i in $(LIBS_TO_VISIT); do \
	  $(MAKE) $(VERBOSE_DIR) -f $(BASE_DIR)/mk/dep_lib.mk REP_DIR=$(REP_DIR) LIB=$$i; done
	@(echo ""; \
	  echo "ifeq (\$$(filter \$$(DEP_$(TARGET).prg:.lib=),\$$(INVALID_DEPS)),)"; \
	  echo "all: $(TARGET).prg"; \
	  echo "endif") >> $(LIB_DEP_FILE)
