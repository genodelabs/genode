#
# Specifics for the NOVA kernel API
#

SPECS += nova
SPECS += pci ps2 vesa framebuffer

#
# We would normally have to do this only in the kernel lib. We do it in
# general nonetheless to ensure that the kernel port, if missing, is added to
# the missing-ports list of the first build stage. The kernel lib is evaluated
# only at a later build stage.
#
NOVA_SRC_DIR := $(call select_from_ports,nova)/src/kernel/nova
