PORT_DIR := $(call port_dir,$(REP_DIR)/ports/dde_bsd)

MK_FILES := dde_bsd_audio.inc dde_bsd_audio_include.mk dde_bsd_audio_pci.inc

LIB_MK := $(addprefix lib/mk/, $(MK_FILES)) \
          $(foreach SPEC,x86_32 x86_64,lib/mk/spec/$(SPEC)/dde_bsd_audio.mk) \
          lib/import/import-dde_bsd_audio_include.mk

MIRROR_FROM_REP_DIR := $(LIB_MK) src/lib src/drivers patches include

MIRROR_FROM_PORT_DIR := $(addprefix src/lib/audio/, \
                          dev/pci/azalia_codec.c \
                          dev/pci/pcidevs.h \
                          dev/pci/pcidevs_data.h \
                          dev/pci/azalia.h \
                          dev/pci/azalia.c \
                          dev/mulaw.h \
                          dev/audio_if.h \
                          dev/mulaw.c \
                          dev/audio.c \
                          lib/libkern \
                          sys/device.h \
                          sys/audioio.h \
                          sys/queue.h)

content: $(MIRROR_FROM_REP_DIR) $(MIRROR_FROM_PORT_DIR) LICENSE

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

$(MIRROR_FROM_PORT_DIR):
	mkdir -p $(dir $@)
	cp -r $(PORT_DIR)/$@ $@

LICENSE:
	( echo 'OpenBSD code is most likely licensed under a ISC like license but for'; \
	  echo 'historical reasons different variations of the Berkeley Copyright are'; \
	  echo 'also in use. Please check the copyright header in the contrib source'; \
	  echo 'files in src/lib/audio/dev and src/lib/audio/sys.') > $@
