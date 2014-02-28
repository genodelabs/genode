include ports/libvorbis.inc

LIBVORBIS_BASE_URL = http://downloads.xiph.org/releases/vorbis

LIBVORBIS_TBX = $(LIBVORBIS).tar.xz
LIBVORBIS_SHA = $(LIBVORBIS)-SHA256SUMS

LIBVORBIS_TBX_URL = $(LIBVORBIS_BASE_URL)/$(LIBVORBIS_TBX)
LIBVORBIS_SHA_URL = $(LIBVORBIS_BASE_URL)/SHA256SUMS


#
# Interface to top-level prepare Makefile
#
PORTS +=$(LIBVORBIS)

prepare-libvorbis: $(CONTRIB_DIR)/$(LIBVORBIS) include/vorbis

$(CONTRIB_DIR)/$(LIBVORBIS):clean-libvorbis


#
# Port-specific local rules
#
$(DOWNLOAD_DIR)/$(LIBVORBIS_TBX):
	$(VERBOSE)wget -c -P $(DOWNLOAD_DIR) $(LIBVORBIS_TBX_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBVORBIS_SHA):
	$(VERBOSE)wget -c -O $(DOWNLOAD_DIR)/$(LIBVORBIS_SHA) $(LIBVORBIS_SHA_URL) && touch $@

$(DOWNLOAD_DIR)/$(LIBVORBIS_TBX).verified: $(DOWNLOAD_DIR)/$(LIBVORBIS_TBX) $(DOWNLOAD_DIR)/$(LIBVORBIS_SHA)
	$(VERBOSE)$(HASHVERIFIER) $(DOWNLOAD_DIR)/$(LIBVORBIS_TBX) $(DOWNLOAD_DIR)/$(LIBVORBIS_SHA) sha256
	$(VERBOSE)touch $@

$(CONTRIB_DIR)/$(LIBVORBIS): $(DOWNLOAD_DIR)/$(LIBVORBIS_TBX)
	$(VERBOSE)tar xfJ $(DOWNLOAD_DIR)/$(LIBVORBIS_TBX) -C $(CONTRIB_DIR) && touch $@

#
# Install Worbis headers
#
include/vorbis:
	$(VERBOSE)mkdir -p $@
	$(VERBOSE)ln -fs \
		../../$(CONTRIB_DIR)/$(LIBVORBIS)/include/vorbis/codec.h \
		../../$(CONTRIB_DIR)/$(LIBVORBIS)/include/vorbis/vorbisenc.h \
		../../$(CONTRIB_DIR)/$(LIBVORBIS)/include/vorbis/vorbisfile.h \
			$@

clean-libvorbis:
	$(VERBOSE)rm -rf \
		include/vorbis/codec.h \
		include/vorbis/vorbisenc.h \
		include/vorbis/vorbisfile.h
	$(VERBOSE)rmdir include/vorbis 2>/dev/null || true
	$(VERBOSE)rm -rf $(CONTRIB_DIR)/$(LIBVORBIS)

