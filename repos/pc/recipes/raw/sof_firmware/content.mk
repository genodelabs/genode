PORT_DIR := $(call port_dir,$(REP_DIR)/ports/sof)

SOF_DIR           := $(PORT_DIR)/firmware/sof/intel-signed
SOF_TPLG_DIR      := $(PORT_DIR)/firmware/sof-tplg
SOF_IPC4_DIR      := $(PORT_DIR)/firmware/sof-ipc4
SOF_IPC4_TPLG_DIR := $(PORT_DIR)/firmware/sof-ipc4-tplg

content: ucode_files LICENSE.Intel sof_firmware.tar

.PHONY: ucode_files

ucode_files:
	$(VERBSE)mkdir -p intel/sof && \
	         mkdir -p intel/sof-ace-tplg && \
	         mkdir -p intel/sof-ipc4 && \
	         mkdir -p intel/sof-ipc4/arl && \
	         mkdir -p intel/sof-ipc4/mtl && \
	         mkdir -p intel/sof-tplg
	$(VERBOSE)cp $(SOF_DIR)/sof-adl.ri intel/sof/sof-adl.ri
	$(VERBOSE)cp $(SOF_DIR)/sof-cml.ri intel/sof/sof-cml.ri
	$(VERBOSE)cp $(SOF_DIR)/sof-cnl.ri intel/sof/sof-cnl.ri
	$(VERBOSE)cp $(SOF_DIR)/sof-adl.ri intel/sof/sof-rpl.ri
	$(VERBOSE)cp $(SOF_DIR)/sof-tgl.ri intel/sof/sof-tgl.ri
	$(VERBOSE)cp $(SOF_TPLG_DIR)/sof-hda-generic-2ch.tplg intel/sof-tplg
	$(VERBOSE)cp $(SOF_IPC4_DIR)/arl/sof-arl.ri intel/sof-ipc4/arl/sof-arl.ri
	$(VERBOSE)cp $(SOF_IPC4_DIR)/mtl/sof-mtl.ri intel/sof-ipc4/mtl/sof-mtl.ri
	$(VERBOSE)cp $(SOF_IPC4_TPLG_DIR)/sof-hda-generic-2ch.tplg intel/sof-ace-tplg

LICENSE.Intel:
	cp $(PORT_DIR)/firmware/LICENCE.Intel $@

include $(GENODE_DIR)/repos/base/recipes/content.inc

sof_firmware.tar: ucode_files LICENSE.Intel
	$(TAR) --remove-files -cf $@ -C . *

