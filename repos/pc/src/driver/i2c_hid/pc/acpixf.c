/*
 * \brief  Linux emulation environment: ACPICA utilities
 * \author Christian Helmuth
 * \date   2022-06-08
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <lx_emul.h>
#include <lx_emul/acpi.h>

#include <linux/acpi.h>
#include <acpi/acpixf.h>


/* from drivers/acpi/acpica/utglobal.c */
struct acpi_table_fadt acpi_gbl_FADT;



acpi_status acpi_buffer_to_resource(u8 *aml_buffer, u16 aml_buffer_length,
                                     struct acpi_resource **resource_ptr)
{
	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_resource_to_address64(struct acpi_resource *resource,
                                       struct acpi_resource_address64 *out)
{
	lx_emul_trace_and_stop(__func__);
}



static union acpi_object * prepare_buffer(struct acpi_buffer *buf,
                                          unsigned obj_count)
{
	union acpi_object *obj;

	if (buf->length == ACPI_ALLOCATE_BUFFER) {
		size_t const len = sizeof(*obj)*obj_count;

		buf->length  = len;
		buf->pointer = acpi_os_allocate(len);
	}

	obj = buf->pointer;
	memset(obj, 0, sizeof(*obj)*obj_count);

	return obj;
}


acpi_status acpi_evaluate_object(acpi_handle handle, acpi_string pathname,
                                 struct acpi_object_list *external_params,
                                 struct acpi_buffer *return_buffer)
{
	union acpi_object *obj;

	if (ACPI_COMPARE_NAMESEG(pathname, "_DSM")
	 && !strcmp(lx_emul_acpi_name(handle), "EPTP")) {
		obj = prepare_buffer(return_buffer, 1);

		obj[0].type = ACPI_TYPE_INTEGER;
		obj[0].integer.value = i2c_hid_config.hid_addr;

		return (AE_OK);
	}

	return (AE_NULL_OBJECT);
}


acpi_status acpi_install_address_space_handler(acpi_handle device,
                                               acpi_adr_space_type space_id,
                                               acpi_adr_space_handler handler,
                                               acpi_adr_space_setup setup,
                                               void *context)
{
	lx_emul_trace(__func__);
	return (AE_OK);
}


acpi_status acpi_remove_address_space_handler(acpi_handle device,
                                              acpi_adr_space_type space_id,
                                              acpi_adr_space_handler handler)
{
	lx_emul_trace(__func__);
	return (AE_OK);
}


acpi_status acpi_get_handle(acpi_handle parent, const char* pathname,
                            acpi_handle *ret_handle)
{
	*ret_handle = (acpi_handle)2;

	return (AE_OK);
}


acpi_status acpi_walk_resources(acpi_handle device_handle, char *name,
                                acpi_walk_resource_callback user_function,
                                void *context)
{
	/* pretend absence of "5.6.5 GPIO-signaled ACPI Events" */
	if (ACPI_COMPARE_NAMESEG(name, METHOD_NAME__AEI))
		return (AE_NOT_FOUND);

	/* XXX only _CRS for now */
	if (!ACPI_COMPARE_NAMESEG(name, METHOD_NAME__CRS))
		return (AE_NOT_FOUND);

	return (AE_OK);
}


struct handle_list_entry {
	struct list_head node;
	void *handle;
};


static void * walk_namespace_cb(void *handle, char const *name,
                                unsigned res_array_elems,
                                struct lx_emul_acpi_resource *res_array,
                                void *context)
{
	struct list_head *list = context;

	struct handle_list_entry *e = kzalloc(sizeof(*e), GFP_KERNEL);

	if (e) {
		e->handle = handle;
		list_add(&e->node, list);
	}

	return NULL;
}


acpi_status acpi_walk_namespace(acpi_object_type type, acpi_handle start_object, u32 max_depth,
                                acpi_walk_callback descending_callback,
                                acpi_walk_callback ascending_callback,
                                void *context, void **return_value)
{
	struct handle_list_entry *entry;

	LIST_HEAD(handle_list);

	if (type != ACPI_TYPE_DEVICE)
		return (AE_OK);

	lx_emul_acpi_for_each_device(walk_namespace_cb, &handle_list);

	list_for_each_entry(entry, &handle_list, node) {
		descending_callback(entry->handle, 0, context, return_value);
	}

	/* TODO dealloc list elements */

	return (AE_OK);
}


struct object_data {
	/* TODO list head for multiple data pointers */
	struct list_head     list;
	acpi_object_handler  handler;
	void                *data;
};


acpi_status acpi_attach_data(acpi_handle obj_handle, acpi_object_handler handler, void *data)
{
	struct object_data *head = lx_emul_acpi_object(obj_handle);

	struct object_data *obj_data = kzalloc(sizeof(*obj_data), GFP_KERNEL);

	INIT_LIST_HEAD(&obj_data->list);
	obj_data->handler = handler;
	obj_data->data    = data;

	if (head) {
		/* TODO list head for multiple data pointers */
		printk("%s: implement for more than one data pointer\n", __func__);
		lx_emul_trace_and_stop(__func__);
	} else {
		lx_emul_acpi_set_object(obj_handle, obj_data);
	}

	return (AE_OK);
}


acpi_status acpi_detach_data(acpi_handle obj_handle, acpi_object_handler handler)
{
	struct object_data *head = lx_emul_acpi_object(obj_handle);

	if (!head)
		return (AE_NOT_FOUND);

	lx_emul_trace_and_stop(__func__);
}


acpi_status acpi_get_data(acpi_handle obj_handle, acpi_object_handler handler, void **data)
{
	struct object_data *head = lx_emul_acpi_object(obj_handle);

	if (!head)
		return (AE_NOT_FOUND);

	if (head->handler != handler) {
		/* TODO list head for multiple data pointers */
		printk("%s: implement for more than one data pointer\n", __func__);
		lx_emul_trace_and_stop(__func__);
	} else {
		*data = head->data;
	}

	return (AE_OK);
}
