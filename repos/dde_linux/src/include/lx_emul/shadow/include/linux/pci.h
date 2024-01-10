/**
 * \brief  Shadow copy of linux/pci.h
 * \author Josef Soentgen
 * \date   2022-01-14
 */

#ifndef _LINUX__PCI_H_
#define _LINUX__PCI_H_

#include <lx_emul/init.h>

#include_next <linux/pci.h>

#undef DECLARE_PCI_FIXUP_CLASS_FINAL

#define DECLARE_PCI_FIXUP_CLASS_FINAL(vendor, device, class, \
                     class_shift, hook)                      \
static void __pci_fixup_final_##hook(struct pci_dev *dev) {  \
	lx_emul_register_pci_fixup(hook, __func__); };             \
	void * __PASTE(__initptr_pci_fixup_final_##hook##_,        \
	       __PASTE(__COUNTER__,                                \
	       __PASTE(_, __LINE__))) = __pci_fixup_final_##hook;



#endif /* _LINUX__PCI_H_ */
