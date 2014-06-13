TARGET = maelstrom.tar

include $(REP_DIR)/ports/maelstrom.port

MAELSTROM_DIR := $(call select_from_ports,maelstrom)/src/app/maelstrom

maelstrom.tar:
	$(VERBOSE)cp -ur \
		$(MAELSTROM_DIR)/Images/ \
		$(MAELSTROM_DIR)/sounds/ \
		$(MAELSTROM_DIR)/icon.bmp \
		$(MAELSTROM_DIR)/Maelstrom_Fonts \
		$(MAELSTROM_DIR)/Maelstrom-Scores \
		$(MAELSTROM_DIR)/Maelstrom_Sprites \
			$(CURDIR)

	$(VERBOSE)tar cf $@ .

$(INSTALL_DIR)/$(TARGET): $(TARGET)
	$(VERBOSE)ln -sf $(CURDIR)/$(TARGET) $@
