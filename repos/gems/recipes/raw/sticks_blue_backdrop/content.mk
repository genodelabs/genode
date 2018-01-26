content: backdrop.config

backdrop.config:
	cp $(REP_DIR)/recipes/raw/sticks_blue_backdrop/$@ $@

content: sticks_blue.png

sticks_blue.png:
	wget --quiet https://genode.org/files/turmvilla/sticks_blue.png
