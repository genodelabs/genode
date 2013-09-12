#
# \brief  Download integrate Linux kernel sources with Genode
# \author Norman Feske
# \date   2012-01-28

CONTRIB_DIR  = contrib
DOWNLOAD_DIR = download
VERBOSE     ?= @
ECHO         = @echo
PATCHES      := $(shell find patches -name \*.patch)

LINUX      = linux-3.9
LINUX_TBZ2 = $(LINUX).tar.bz2
LINUX_URL  = http://www.kernel.org/pub/linux/kernel/v3.x/$(LINUX_TBZ2)

# Linux utilities
CONTENT += include/linux/list.h
CONTENT += $(addprefix include/linux/,pci_ids.h usb.h hid.h hiddev.h input.h mod_devicetable.h)
CONTENT += include/linux/byteorder/generic.h
CONTENT += include/linux/swab.h

CONTENT += $(addprefix include/asm-generic/bitops/,__ffs.h non-atomic.h)

CONTENT_UAPI  = byteorder/little_endian.h hid.h input.h pci_regs.h usb/ch11.h \
                usb/ch9.h
CONTENT_USB = ch9.h phy.h
CONTENT    += $(addprefix include/uapi/linux/,$(CONTENT_UAPI))
CONTENT    += $(addprefix include/linux/usb/,$(CONTENT_USB))
CONTENT    += include/uapi/asm-generic/ioctl.h

# USB core
CONTENT_CORE = buffer.c config.c devices.c driver.c endpoint.c file.c generic.c \
               hcd.c hcd-pci.c hub.h hub.c message.c notify.c quirks.c  port.c  \
               urb.c usb.c usb.h
CONTENT += $(addprefix drivers/usb/core/,$(CONTENT_CORE))
CONTENT += drivers/usb/usb-common.c
CONTENT_INCLUDE_USB := ehci_def.h hcd.h input.h otg.h quirks.h storage.h
CONTENT += $(addprefix include/linux/usb/,$(CONTENT_INCLUDE_USB))

# needed by usb/core/devio.c
CONTENT += include/asm-generic/ioctl.h

# USB host-controller driver
CONTENT_USB_HOST := ehci.h ehci-hcd.c ehci-hub.c ehci-dbg.c ehci-mem.c \
                    ehci-omap.c ehci-q.c ehci-pci.c ehci-s5p.c ehci-sched.c \
                    ehci-sysfs.c ehci-timer.c
CONTENT_USB_HOST += ohci.h ohci-hcd.c ohci-hub.c ohci-dbg.c ohci-mem.c \
                    ohci-q.c ohci-pci.c
CONTENT_USB_HOST += uhci-hcd.h uhci-hcd.c uhci-debug.c uhci-q.c uhci-hub.c \
                    uhci-pci.c
CONTENT_USB_HOST += xhci-dbg.c xhci-ext-caps.h xhci-hub.c xhci-mem.c \
                    xhci-plat.c xhci-ring.c xhci.h xhci.c
CONTENT_USB_HOST += pci-quirks.h pci-quirks.c
CONTENT += $(addprefix drivers/usb/host/,$(CONTENT_USB_HOST))

# USB storage driver
CONTENT += drivers/usb/storage/
CONTENT += include/linux/usb_usual.h

# SCSI support for storage
CONTENT += $(addprefix drivers/scsi/,scsi.h scsi.c constants.c scsi_priv.h scsi_logging.h)
CONTENT += $(addprefix include/scsi/,scsi.h scsi_host.h)

# USB hid driver
CONTENT += drivers/hid/hid-input.c drivers/hid/hid-generic.c drivers/hid/hid-core.c drivers/hid/hid-ids.h
CONTENT += drivers/hid/hid-cherry.c drivers/hid/usbhid

# needed by USB hid
CONTENT_INPUT := input.c evdev.c input-compat.h
CONTENT += $(addprefix drivers/input/,$(CONTENT_INPUT))
CONTENT += include/linux/input/mt.h

# usb network driver
CONTENT_NET = usbnet.c
# Panda board
CONTENT_NET += smsc95xx.c smsc95xx.h
# Arndale board
CONTENT_NET += asix_devices.c asix_common.c ax88172a.c ax88179_178a.c asix.h

CONTENT    += $(addprefix drivers/net/usb/,$(CONTENT_NET))
CONTENT    += include/linux/usb/usbnet.h include/linux/netdev_features.h

# DWC3 controller
CONTENT_DWC3 = core.c core.h dwc3-exynos.c host.c io.h
CONTENT     += $(addprefix drivers/usb/dwc3/,$(CONTENT_DWC3))
CONTENT     += include/linux/platform_data/dwc3-exynos.h

# OMAP
CONTENT    += include/linux/platform_data/usb-omap.h

# Arndale
CONTENT += arch/arm/plat-samsung/include/plat/usb-phy.h
CONTENT += include/linux/platform_data/usb-ehci-s5p.h

# Raspberry Pi
DWC_OTG_GIT_URL    := https://github.com/nfeske/dwc_otg.git
DWC_OTG_GIT_BRANCH := r1

#
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
	$(ECHO) "Download integrate Linux kernel sources with Genode"
	$(ECHO)
	$(ECHO) "--- available commands ---"
	$(ECHO) "prepare  - download and integrate Linux source code"
	$(ECHO) "clean    - remove contib sources except downloaded archives"
	$(ECHO) "cleanall - remove contib sources and downloaded archives"
	$(ECHO)

prepare: clean $(CONTRIB_DIR)/.prepared

prepare_rpi: prepare
	$(VERBOSE)cd $(CONTRIB_DIR)/drivers/usb/host; \
	    git clone $(DWC_OTG_GIT_URL) dwc_otg
	$(VERBOSE)cd $(CONTRIB_DIR)/drivers/usb/host/dwc_otg; \
	    git reset --hard HEAD && git checkout $(DWC_OTG_GIT_BRANCH)

$(CONTRIB_DIR)/.prepared: Makefile
$(CONTRIB_DIR)/.prepared: $(DOWNLOAD_DIR)/$(LINUX_TBZ2)
	$(ECHO) "extracting source code to '$(CONTRIB_DIR)'"
	$(VERBOSE)tar xfj $< --transform "s/$(LINUX)/$(CONTRIB_DIR)/" $(addprefix $(LINUX)/,$(CONTENT))
	$(VERBOSE)touch $@
	$(ECHO) "applying patches to '$(CONTRIB_DIR)/'"
	$(VERBOSE)for i in $(PATCHES); do patch -d $(CONTRIB_DIR) -p1 < $$i; done
	$(VERBOSE)ln -sf ../../uapi/linux/usb/ch11.h $(CONTRIB_DIR)/include/linux/usb/ch11.h
	$(VERBOSE)touch $(CONTRIB_DIR)/drivers/usb/dwc3/gadget.h
	$(VERBOSE)touch $(CONTRIB_DIR)/drivers/usb/dwc3/debug.h


$(DOWNLOAD_DIR):
	$(VERBOSE)mkdir -p $@

$(DOWNLOAD_DIR)/$(LINUX_TBZ2): $(DOWNLOAD_DIR)
	$(ECHO) "downloading source code to '$@'"
	$(VERBOSE)cd $(DOWNLOAD_DIR); wget -c $(LINUX_URL)
	$(VERBOSE)touch $@

clean:
	$(VERBOSE)rm -rf $(CONTRIB_DIR)

cleanall: clean
	$(VERBOSE)rm -rf $(DOWNLOAD_DIR)
