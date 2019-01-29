include $(REP_DIR)/lib/import/import-spark.mk

ADA_RTS_SOURCE = $(call select_from_ports,gcc)/src/noux-pkg/gcc/gcc/ada

all: ada_source_path

ada_source_path: ada_object_path
	$(VERBOSE)echo $(ADA_RTS_SOURCE) > $(ADA_RTS)/ada_source_path

ada_object_path:
	$(VERBOSE)mkdir -p $(ADA_RTS)
	$(VERBOSE)echo $(ADA_RTS) > $(ADA_RTS)/ada_object_path
