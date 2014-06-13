include ports/maelstrom.inc

MAELSTROM_TGZ = $(MAELSTROM).tar.gz
MAELSTROM_URL = https://github.com/ehmry/maelstrom/archive/$(MAELSTROM_VER).tar.gz

PORTS += $(MAELSTROM)

prepare:: $(CONTRIB_DIR)/$(MAELSTROM) $(CONTRIB_DIR)/$(MAELSTROM)/sound.h

$(DOWNLOAD_DIR)/$(MAELSTROM_TGZ):
	$(VERBOSE)wget -c -O $(DOWNLOAD_DIR)/$(MAELSTROM_TGZ) $(MAELSTROM_URL) && touch $@

$(CONTRIB_DIR)/$(MAELSTROM): $(DOWNLOAD_DIR)/$(MAELSTROM_TGZ)
	$(VERBOSE)tar xfz $(<) -C $(CONTRIB_DIR) && touch $@

$(CONTRIB_DIR)/$(MAELSTROM)/sound.h: $(CONTRIB_DIR)/$(MAELSTROM)
	$(VERBOSE)patch -p1 -d $(CONTRIB_DIR)/$(MAELSTROM) < src/app/maelstrom/audio-conversion.patch
