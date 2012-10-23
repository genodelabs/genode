#
# \brief  Download and setup OSS sources
# \author Sebastian Sumpf
# \date   2012-08-30

CONTRIB_DIR  = contrib
DOWNLOAD_DIR = download
VERBOSE     ?= @
ECHO         = @echo
CC          ?= gcc
PATCHES      := $(shell find patches -name \*.patch)

OSS      = oss-v4.2-build2006-src-bsd
OSS_TBZ2 = $(OSS).tar.bz2
OSS_URL  = http://www.4front-tech.com/developer/sources/stable/bsd/$(OSS_TBZ2)


# needed for preparation
CONTENT_SETUP = $(addprefix setup/,srcconf.c srcconf_freebsd.inc srcconf_vxworks.inc gen_driver_freebsd.inc)
CONTENT += $(CONTENT_SETUP)

# oss framework
CONTENT_FRAMEWORK = $(addprefix kernel/framework/include/,oss_config.h oss_memblk.h \
                                oss_version.h audio_core.h mixer_core.h oss_calls.h \
                                internal.h oss_pci.h spdif.h midi_core.h grc3.h ac97.h \
                                ossddk/oss_exports.h ossddk/oss_limits.PHh ossddk/ossddk.h)
CONTENT += $(CONTENT_FRAMEWORK)

# oss core
CONTENT_CORE = $(addprefix kernel/framework/osscore/,oss_memblk.c oss_core_options.c \
                                                     oss_core_services.c)
CONTENT += $(CONTENT_CORE)
CONTENT += include/soundcard.h kernel/drv/.config

# audio core
CONTENT_AUDIO = $(addprefix kernel/framework/audio/,oss_audio_core.c oss_spdif.c oss_audiofmt.c \
                                                    ulaw.h audiocnv.inc oss_grc3.c fltdata2_h.inc \
                                                    grc3code.inc grc3inc.inc)
CONTENT += $(CONTENT_AUDIO)

# mixer core
CONTENT_MIXER = $(addprefix kernel/framework/mixer/,oss_mixer_core.c mixerdefs.h)
CONTENT += $(CONTENT_MIXER)

# vmixer core
CONTENT_VMIX = $(addprefix kernel/framework/vmix_core/,vmix_core.c vmix_input.c vmix.h db_scale.h \
                                                       vmix_import.inc  vmix_import_int.inc \
                                                       rec_export.inc rec_export_int.inc \
                                                       vmix_output.c outexport.inc outexport_int.inc \
                                                       playmix.inc playmix_int.inc playmix_src.inc)
CONTENT += $(CONTENT_VMIX)

# midi core
CONTENT_MIDI = $(addprefix kernel/framework/midi/,oss_midi_core.c oss_midi_timers.c oss_midi_mapper.c \
                                                  oss_midi_queue.c)
CONTENT += $(CONTENT_MIDI)

# AC97 core
CONTENT += kernel/framework/ac97

# drivers
CONTENT_DRV += oss_ich oss_hdaudio oss_audiopci
CONTENT += $(addprefix kernel/drv/,$(CONTENT_DRV))


#
# Utility to check if a tool is installed
#
check_tool = $(if $(shell which $(1)),,$(error Need to have '$(1)' installed.))

$(call check_tool,wget)
$(call check_tool,patch)

#
# Print help information by default
#
help:
	$(ECHO)
	$(ECHO) "Download integrate OSS sources with Genode"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and integrate OSS source code"
	$(ECHO) "clean    - remove contib sources except downloaded archives"
	$(ECHO) "cleanall - remove contib sources and downloaded archives"
	$(ECHO)

#
# Build and execute 'srcconf' utility, build 'devices.list'
#
setup:
	$(VERBOSE)ln -sf srcconf_freebsd.inc $(CONTRIB_DIR)/setup/srcconf_linux.inc
	$(VERBOSE)ln -sf gen_driver_freebsd.inc $(CONTRIB_DIR)/setup/gen_driver_linux.inc
	$(VERBOSE)mkdir -p $(CONTRIB_DIR)/kernel/framework/include
	$(VERBOSE)mkdir -p $(CONTRIB_DIR)/kernel/OS/Linux
	$(VERBOSE)$(CC) -g -I$(CONTRIB_DIR)/setup -o srcconf $(CONTRIB_DIR)/setup/srcconf.c
	$(VERBOSE)cat `find $(CONTRIB_DIR)/kernel/drv -name .devices`| grep -v '^#' > $(CONTRIB_DIR)/devices.list
	$(VERBOSE)cd $(CONTRIB_DIR) && ../srcconf
	$(VERBOSE)cd $(CONTRIB_DIR)/target/build ; for f in *.c; do mv $$f pci_$$f; done
	$(VERBOSE)rm srcconf

prepare: $(CONTRIB_DIR)/.prepared setup

$(CONTRIB_DIR)/.prepared: Makefile
$(CONTRIB_DIR)/.prepared: $(DOWNLOAD_DIR)/$(OSS_TBZ2)
	$(ECHO) "extracting source code to '$(CONTRIB_DIR)'"
	$(VERBOSE)tar xfj $< --transform "s/$(OSS)/$(CONTRIB_DIR)/" $(addprefix $(OSS)/,$(CONTENT))
	$(VERBOSE)touch $@
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $(CONTRIB_DIR) -p1 < $$i; done


$(DOWNLOAD_DIR):
	$(VERBOSE)mkdir -p $@

$(DOWNLOAD_DIR)/$(OSS_TBZ2): $(DOWNLOAD_DIR)
	$(ECHO) "downloading source code to '$@'"
	$(VERBOSE)cd $(DOWNLOAD_DIR); wget -c $(OSS_URL)
	$(VERBOSE)touch $@

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
