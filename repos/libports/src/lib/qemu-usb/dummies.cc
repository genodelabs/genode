/*
 * \brief  Qemu USB controller interface
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2015-12-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/log.h>

/* local includes */
#include <extern_c_begin.h>
#include <qemu_emul.h>
#include <extern_c_end.h>


extern "C" {

enum {
	SHOW_TRACE = 0,
};

#define TRACE_AND_STOP \
	do { \
		Genode::warning(__func__, " not implemented called from: ", __builtin_return_address(0)); \
		Genode::sleep_forever(); \
	} while (0)

#define TRACE \
	do { \
		if (SHOW_TRACE) \
			Genode::warning(__func__, " not implemented"); \
	} while (0)


/****************
 ** hcd-xhci.c **
 ****************/

void memory_region_del_subregion(MemoryRegion*, MemoryRegion*)
{
	TRACE_AND_STOP;
}


void msix_vector_unuse(PCIDevice*, unsigned int)
{
	TRACE;
}


int msix_vector_use(PCIDevice*, unsigned int)
{
	TRACE;
	return 0;
}


ObjectClass* object_class_dynamic_cast_assert(ObjectClass*, const char*, const char*, int,
                                              const char*)
{
	TRACE_AND_STOP;
	return nullptr;
}


Object* object_dynamic_cast_assert(Object*, const char*, const char*, int, const char*)
{
	TRACE_AND_STOP;
	return nullptr;
}


PCIBus *pci_get_bus(const PCIDevice *dev)
{
	TRACE;
	return nullptr;
}


bool pci_bus_is_express(PCIBus*)
{
	TRACE;
	return false;
}


void pci_register_bar(PCIDevice*, int, uint8_t, MemoryRegion*)
{
	TRACE;
}


int pcie_endpoint_cap_init(PCIDevice*, uint8_t)
{
	TRACE_AND_STOP;
	return -1;
}


/***********
 ** bus.c **
 ***********/

static Error _error;
Error *error_abort = &_error;


ObjectClass* object_get_class(Object*)
{
	TRACE_AND_STOP;
	return nullptr;
}

const char* object_get_typename(Object*)
{
	TRACE;
	return "";
}


void qbus_set_bus_hotplug_handler(BusState *state)
{
	TRACE;
}


gchar* g_strdup_printf(const gchar*, ...)
{
	TRACE;
	return 0;
}


void pstrcpy(char*, int, const char*)
{
	TRACE;
}


long int strtol(const char*, char**, int)
{
	TRACE;
	return -1;
}


ObjectProperty *object_property_add_bool(Object *, const char *,
                                         bool (*get)(Object *, Error **),
                                         void (*set)(Object *, bool, Error **))
{
	TRACE_AND_STOP;
	return nullptr;
}


const PropertyInfo qdev_prop_link = { 0 };
const PropertyInfo qdev_prop_on_off_auto = { 0 };


void qdev_alias_all_properties(DeviceState *target, Object *source)
{
	TRACE;
}


DeviceState* qdev_try_create(BusState*, const char*)
{
	TRACE_AND_STOP;
	return nullptr;
}


DeviceState *qdev_new(const char *name)
{
	TRACE_AND_STOP;
	return nullptr;
}


DeviceState *qdev_try_new(const char *name)
{
	TRACE_AND_STOP;
	return nullptr;
}


bool qdev_realize_and_unref(DeviceState *, BusState *, Error **)
{
	TRACE_AND_STOP;
	return false;
}


void monitor_printf(Monitor*, const char*, ...)
{
	TRACE;
}


void qdev_simple_device_unplug_cb(HotplugHandler*, DeviceState*, Error**)
{
	TRACE_AND_STOP;
}


char* qdev_get_dev_path(DeviceState*)
{
	TRACE_AND_STOP;
	return 0;
}


const char* qdev_fw_name(DeviceState*)
{
	TRACE;
	return 0;
}


AddressSpace *pci_get_address_space(PCIDevice *dev)
{
	TRACE;
	return nullptr;
}


void object_initialize_child_internal(Object *parent, const char *propname,
                                      void *child, size_t size,
                                      const char *type)
{
	TRACE;
}


bool object_property_set_link(Object *obj, const char *name,
                              Object *value, Error **errp)
{
	TRACE;
	return true;
}


gchar* g_strdup(const gchar*)
{
	TRACE;
	return 0;
}


size_t strlen(const char*)
{
	TRACE_AND_STOP;
	return 0;
}


/**********
 ** libc **
 **********/

void abort() { TRACE_AND_STOP; }

} /* extern "C" */
