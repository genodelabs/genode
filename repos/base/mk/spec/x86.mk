ifeq ($(filter $(SPECS),linux),)
SPECS += vesa pci ps2 framebuffer usb
endif
