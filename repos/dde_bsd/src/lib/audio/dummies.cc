/**
 * \brief  Dummy functions
 * \author Josef Soentgen
 * \date   2014-11-13
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

extern "C" {
	typedef long DUMMY;

enum {
	SHOW_DUMMY = 0,
	SHOW_SKIP  = 0,
	SHOW_RET   = 0,
};

#define DUMMY(retval, name) \
DUMMY name(void) { \
	if (SHOW_DUMMY) \
		Genode::warning( #name " called (from ", __builtin_return_address(0), ") not implemented"); \
	return retval; \
}

#define DUMMY_SKIP(retval, name) \
DUMMY name(void) { \
	if (SHOW_SKIP) \
		Genode::warning( #name " called (from ", __builtin_return_address(0), ") skipped"); \
	return retval; \
}

#define DUMMY_RET(retval, name) \
DUMMY name(void) { \
	if (SHOW_RET) \
		Genode::warning( #name " called (from ", __builtin_return_address(0), ") return ", retval); \
	return retval; \
}

DUMMY_RET(1, pci_intr_map_msi) /* do not support MSI API */
DUMMY_RET(0, pci_intr_string)

DUMMY(0, bus_dmamap_unload)
DUMMY(0, bus_dmamem_mmap)
DUMMY(0, bus_dmamem_unmap)
DUMMY(0, bus_space_unmap)
DUMMY(0, config_activate_children)
DUMMY(0, config_deactivate)
DUMMY(0, config_detach)
DUMMY(0, config_detach_children)
DUMMY(0, cpu_info_primary)
DUMMY(0, device_unref)
DUMMY(0, klist_free)
DUMMY(0, klist_init_mutex)
DUMMY(0, klist_insert_locked)
DUMMY(0, klist_invalidate)
DUMMY(0, klist_remove)
DUMMY(0, klist_remove_locked)
DUMMY(0, knote_modify)
DUMMY(0, knote_process)
DUMMY(0, knote_locked)
DUMMY(0, pci_findvendor)
DUMMY(0, pci_intr_disestablish)
DUMMY(0, pci_set_powerstate)
DUMMY(0, psignal)
DUMMY(0, selrecord)
DUMMY(0, selwakeup)
DUMMY(0, softintr_disestablish)
DUMMY(0, softintr_schedule)
DUMMY(0, tsleep)
DUMMY(0, tsleep_nsec)
DUMMY(0, vdevgone)
DUMMY(1, softintr_establish)

} /* extern "C" */
