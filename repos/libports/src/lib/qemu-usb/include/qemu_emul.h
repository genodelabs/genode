/*
 * \brief  Qemu emulation environment
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

#ifndef _INCLUDE__QEMU_EMUL_H_
#define _INCLUDE__QEMU_EMUL_H_

#include <stdarg.h>
#include <base/fixed_stdint.h>

void q_printf(char const *, ...) __attribute__((format(printf, 1, 2)));
void q_vprintf(char const *, va_list);

typedef genode_uint8_t  uint8_t;
typedef genode_uint16_t uint16_t;
typedef genode_uint32_t uint32_t;
typedef genode_uint64_t uint64_t;

typedef genode_int32_t int32_t;
typedef genode_int64_t int64_t;
typedef signed long    ssize_t;

#ifndef __cplusplus
typedef _Bool          bool;
enum { false = 0, true = 1 };
#endif
typedef __SIZE_TYPE__  size_t;
typedef unsigned long  dma_addr_t;
typedef uint64_t       hwaddr;

typedef struct uint16List
{
	union {
		uint16_t value;
	uint64_t padding;
	};
	struct uint16List *next;
} uint16List;


/**********
 ** libc **
 **********/

enum {
	EINVAL = 22,
};

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);

void abort();

#define fprintf(fd, fmt, ...) q_printf(fmt, ##__VA_ARGS__)
int snprintf(char *buf, size_t size, const char *fmt, ...);
int strcmp(const char *s1, const char *s2);
size_t strlen(char const *s);
long int strtol(const char *nptr, char **endptr, int base);
char *strchr(const char *s, int c);


/*************
 ** defines **
 *************/

#define NULL (void *)0
#define QEMU_SENTINEL

#define le32_to_cpu(x) (x)
#define cpu_to_le32(x) (x)
#define cpu_to_le64(x) (x)

#define le32_to_cpus(x)
#define le64_to_cpus(x)

/**
 * Forward declarations
 */

typedef struct Monitor Monitor;
typedef struct QDict QDict;

typedef struct CPUReadMemoryFunc CPUReadMemoryFunc;
typedef struct CPUWriteMemoryFunc CPUWriteMemoryFunc;

typedef struct MemoryRegion { unsigned dummy; } MemoryRegion;

struct tm;


/******************
 ** qapi/error.h **
 ******************/

typedef struct Error { char string[256]; } Error;

void error_setg(Error **errp, const char *fmt, ...);

const char *error_get_pretty(Error *err);
void error_report(const char *fmt, ...);
void error_free(Error *err);
void error_propagate(Error **dst_errp, Error *local_err);

extern Error *error_abort;


/*******************
 ** qemu/bitops.h **
 *******************/

#define BITS_PER_BYTE           8
#define BITS_PER_LONG           (sizeof (unsigned long) * BITS_PER_BYTE)

#define BIT_MASK(nr)            (1UL << ((nr) % BITS_PER_LONG))
#define BIT_WORD(nr)            ((nr) / BITS_PER_LONG)
#define BITS_TO_LONGS(nr)       DIV_ROUND_UP(nr, BITS_PER_BYTE * sizeof(long))
#define DIV_ROUND_UP(n,d)       (((n) + (d) - 1) / (d))

#define DECLARE_BITMAP(name,bits)                  \
        unsigned long name[BITS_TO_LONGS(bits)]

static inline void set_bit(long nr, unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = addr + BIT_WORD(nr);

	*p  |= mask;
}


/*******************
 ** qemu-common.h **
 *******************/

struct iovec
{
	void   *iov_base;
	size_t  iov_len;
};

typedef struct QEMUIOVector
{
	struct iovec  *iov;
	int           niov;
	size_t        size;
	int           alloc_hint;
} QEMUIOVector;

void qemu_iovec_init(QEMUIOVector *qiov, int alloc_hint);
void qemu_iovec_add(QEMUIOVector *qiov, void *base, size_t len);
void qemu_iovec_reset(QEMUIOVector *qiov);
void qemu_iovec_destroy(QEMUIOVector *qiov);

void pstrcpy(char *buf, int buf_size, const char *str);


/****************
 ** qemu/iov.h **
 ****************/

size_t iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
                    size_t offset, const void *buf, size_t bytes);
size_t iov_to_buf(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, void *buf, size_t bytes);
size_t iov_memset(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, int fillc, size_t bytes);

/******************
 ** qom/object.h **
 ******************/

typedef struct Object { unsigned dummy; } Object;
typedef struct ObjectClass { unsigned dummy; } ObjectClass;

struct DeviceState;
struct PCIDevice;
struct XHCIState;

struct PCIDevice   *cast_PCIDevice(void *);
struct XHCIState   *cast_XHCIState(void *);
struct DeviceState *cast_DeviceState(void *);
struct BusState    *cast_BusState(void *);
struct Object      *cast_object(void *);
struct USBDevice   *cast_USBDevice(void *);
struct USBHostDevice *cast_USBHostDevice(void *);

struct PCIDeviceClass      *cast_PCIDeviceClass(void *);
struct DeviceClass         *cast_DeviceClass(void *);
struct BusClass            *cast_BusClass(void *);
struct HotplugHandlerClass *cast_HotplugHandlerClass(void *);

struct USBDeviceClass      *cast_USBDeviceClass(void *);

#define OBJECT_CHECK(type, obj, str) \
	cast_##type((void *)obj)

#define OBJECT_CLASS_CHECK(type, klass, str) \
	OBJECT_CHECK(type, (klass), str)

#define OBJECT(obj) \
	cast_object(obj)

#define OBJECT_GET_CLASS(klass, obj, str) \
	OBJECT_CHECK(klass, obj, str)


typedef struct InterfaceInfo {
	const char *type;
} InterfaceInfo;


typedef struct TypeInfo
{
	char const *name;
	char const *parent;
	size_t      instance_size;

	bool        abstract;
	size_t      class_size;
	void (*class_init)(ObjectClass *klass, void *data);
	InterfaceInfo *interfaces;
} TypeInfo;

struct Type_impl { unsigned dummy; };
typedef struct Type_impl *Type;

Type type_register_static(const TypeInfo *info);

#define object_property_set_bool(...)
#define object_unparent(...)

const char *object_get_typename(Object *obj);

/********************
 ** glib emulation **
 ********************/

void *g_malloc(size_t size);
void  g_free(void *ptr);

#define g_new0(type, count)({ \
	typeof(type) *t = (typeof(type)*)g_malloc(sizeof(type) * count); \
	memset(t, 0, sizeof(type) * count); \
	t; \
	})

#define g_malloc0(size) ({ \
	void *t = g_malloc((size)); \
	memset(t, 0, (size)); \
	t; \
	})

typedef void* gpointer;
typedef struct GSList GSList;

struct GSList
{
	gpointer data;
	GSList *next;
};

GSList *g_slist_append(GSList *list, gpointer data);

typedef char gchar;
gchar *g_strdup(gchar const *str);
gchar *g_strdup_printf(gchar const *fmt, ...);


/********************
 ** hw/qdev-core.h **
 ********************/

typedef enum DeviceCategory {
	DEVICE_CATEGORY_USB = 1,
	DEVICE_CATEGORY_MAX
} DeviceCategory;

typedef struct BusState BusState;

typedef struct DeviceState
{
	BusState *parent_bus;
} DeviceState;

struct VMStateDescription;
struct Property;

typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev, Error **errp);

typedef struct DeviceClass
{
	DECLARE_BITMAP(categories, DEVICE_CATEGORY_MAX);
	struct Property *props;
	void (*reset)(DeviceState *dev);
	DeviceRealize realize;
	DeviceUnrealize unrealize;

	const struct VMStateDescription *vmsd;
	const char *bus_type;
} DeviceClass;

#define DEVICE_CLASS(klass) OBJECT_CHECK(DeviceClass, (klass), "device")


typedef struct BusState
{
	DeviceState *parent;
	char        *name;
} BusState;

typedef struct BusClass
{
	void (*print_dev)(Monitor *mon, DeviceState *dev, int indent);
	char *(*get_dev_path)(DeviceState *dev);
	char *(*get_fw_dev_path)(DeviceState *dev);

} BusClass;

#define TYPE_BUS "bus"
#define BUS(obj) OBJECT_CHECK(BusState, (obj), TYPE_BUS)
#define BUS_CLASS(klass) OBJECT_CLASS_CHECK(BusClass, (klass), TYPE_BUS)

#define TYPE_DEVICE "device"
#define DEVICE(obj) OBJECT_CHECK(DeviceState, (obj), TYPE_DEVICE)

enum Prop_type
{
	BIT, UINT32, END
};

typedef struct Property
{
	enum Prop_type type;
	unsigned       offset;
	unsigned long  value;
} Property;


#define DEFINE_PROP_BIT(_name, _state, _field, _bit, _bool) \
{ .type = BIT, .offset = offsetof(_state, _field), .value = (_bool << _bit) }
#define DEFINE_PROP_UINT32(_name, _state, _field, _value) \
{ .type = UINT32, .offset = offsetof(_state, _field), .value = _value }
#define DEFINE_PROP_END_OF_LIST() { .type = END }
#define DEFINE_PROP_STRING(...) {}


/* forward */
typedef struct DeviceState DeviceState;
typedef struct HotplugHandler HotplugHandler;

void qdev_simple_device_unplug_cb(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp);
void qbus_create_inplace(void *bus, size_t size, const char *typename,
                         DeviceState *parent, const char *name);
void qbus_set_bus_hotplug_handler(BusState *bus, Error **errp);
DeviceState *qdev_create(BusState *bus, const char *name);
DeviceState *qdev_try_create(BusState *bus, const char *name);
char *qdev_get_dev_path(DeviceState *dev);
const char *qdev_fw_name(DeviceState *dev);

/******************
 ** hw/hotplug.h **
 ******************/

typedef struct HotplugHandler { unsigned dummy; } HotplugHandler;

typedef void (*hotplug_fn)(HotplugHandler *plug_handler,
		DeviceState *plugged_dev, Error **errp);

typedef struct HotplugHandlerClass
{
	hotplug_fn unplug;
} HotplugHandlerClass;

#define TYPE_HOTPLUG_HANDLER "hotplug-handler"
#define HOTPLUG_HANDLER_CLASS(klass) \
	OBJECT_CLASS_CHECK(HotplugHandlerClass, (klass), TYPE_HOTPLUG_HANDLER)


/****************
 ** hw/osdep.h **
 ****************/

#define offsetof(type, member) ((size_t) &((type *)0)->member)


#define container_of(ptr, type, member) ({                      \
        const typeof(((type *) 0)->member) *__mptr = (ptr);     \
        (type *) ((char *) __mptr - offsetof(type, member));})

#define DO_UPCAST(type, field, dev) cast_DeviceStateToUSBBus()

struct USBBus *cast_DeviceStateToUSBBus(void);

#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))


/******************
 ** qemu/timer.h **
 ******************/

enum QEMUClockType
{
	QEMU_CLOCK_VIRTUAL = 1,
};

typedef enum QEMUClockType QEMUClockType;
typedef struct QEMUTimer { unsigned dummy; } QEMUTimer;
typedef void QEMUTimerCB(void *opaque);

QEMUTimer *timer_new_ns(QEMUClockType type, QEMUTimerCB *cb, void *opaque);
void       timer_mod(QEMUTimer *ts, int64_t expire_timer);
void       timer_del(QEMUTimer *ts);
void       timer_free(QEMUTimer *ts);

int64_t qemu_clock_get_ns(QEMUClockType type);


/***********************
 ** exec/cpu-common.h **
 ***********************/

enum device_endian { DEVICE_LITTLE_ENDIAN = 2 };


/*******************
 ** exec/memory.h **
 *******************/

typedef struct AddressSpace { unsigned dummy; } AddressSpace;

typedef struct MemoryRegionOps {
	uint64_t (*read)(void *opaque, hwaddr addr, unsigned size);
	void (*write)(void *opaque, hwaddr addr, uint64_t data, unsigned size);

	enum device_endian endianness;

	struct {
		unsigned min_access_size;
		unsigned max_access_size;
	} valid;

	struct {
		unsigned min_access_size;
		unsigned max_access_size;
	} impl;
} MemoryRegionOps;

void memory_region_init(MemoryRegion *mr,
                        struct Object *owner,
                        const char *name,
                        uint64_t size);

void memory_region_init_io(MemoryRegion *mr,
                           struct Object *owner,
                           const MemoryRegionOps *ops,
                           void *opaque,
                           const char *name,
                           uint64_t size);

void memory_region_add_subregion(MemoryRegion *mr,
                                 hwaddr offset,
                                 MemoryRegion *subregion);

void memory_region_del_subregion(MemoryRegion *mr,
                                 MemoryRegion *subregion);

/******************
 ** sysemu/dma.h **
 ******************/

typedef struct QEMUIOVector QEMUSGList;

void qemu_sglist_add(QEMUSGList *qsg, dma_addr_t base, dma_addr_t len);
void qemu_sglist_destroy(QEMUSGList *qsg);

int dma_memory_read(AddressSpace *as, dma_addr_t addr, void *buf, dma_addr_t len);

static inline uint64_t ldq_le_dma(AddressSpace *as, dma_addr_t addr)
{
	uint64_t val;
	dma_memory_read(as, addr, &val, 8);
	return val;
}

static inline uint64_t ldq_le_pci_dma(void *dev, dma_addr_t addr)
{
	return ldq_le_dma(0, addr);
}


/**************
 ** hw/pci.h **
 **************/

enum Pci_regs {
	PCI_BASE_ADDRESS_SPACE_MEMORY = 0,
	PCI_BASE_ADDRESS_MEM_TYPE_64  = 0x04,
	PCI_CLASS_PROG                = 0x09,
	PCI_CACHE_LINE_SIZE           = 0x0c,
	PCI_INTERRUPT_PIN             = 0x3d,
};

enum Pci_ids {
	PCI_CLASS_SERIAL_USB        = 0x0c03,
	PCI_VENDOR_ID_NEC           = 0x1033,
	PCI_DEVICE_ID_NEC_UPD720200 = 0x0194,
};

typedef struct PCIBus { unsigned dummy; } PCIBus;

typedef struct PCIDevice
{
	uint8_t config[0x1000]; /* PCIe config space */
	PCIBus *bus;

	uint8_t *msix_table;
	uint8_t *msix_pba;

	MemoryRegion msix_table_mmio;
	MemoryRegion msix_pba_mmio;

	unsigned *msix_entry_used;
} PCIDevice;

typedef void PCIUnregisterFunc(PCIDevice *pci_dev);

typedef struct PCIDeviceClass
{
	void (*realize)(PCIDevice *dev, Error **errp);
	PCIUnregisterFunc *exit;

	uint16_t vendor_id;
	uint16_t device_id;
	uint8_t  revision;
	uint16_t class_id;

	int is_express;

} PCIDeviceClass;

#define TYPE_PCI_DEVICE "pci-device"
#define PCI_DEVICE(obj) \
        OBJECT_CHECK(PCIDevice, (obj), TYPE_PCI_DEVICE)
#define PCI_DEVICE_CLASS(klass) \
        OBJECT_CLASS_CHECK(PCIDeviceClass, (klass), TYPE_PCI_DEVICE)

int pci_dma_read(PCIDevice *dev, dma_addr_t addr,
                 void *buf, dma_addr_t len);

int pci_dma_write(PCIDevice *dev, dma_addr_t addr,
                  const void *buf, dma_addr_t len);

void pci_set_irq(PCIDevice *pci_dev, int level);
void pci_irq_assert(PCIDevice *pci_dev);
void pci_dma_sglist_init(QEMUSGList *qsg, PCIDevice *dev,
                         int alloc_hint);
AddressSpace *pci_get_address_space(PCIDevice *dev);

void pci_register_bar(PCIDevice *pci_dev, int region_num,
                      uint8_t attr, MemoryRegion *memory);
bool pci_bus_is_express(PCIBus *bus);

int pcie_endpoint_cap_init(PCIDevice *dev, uint8_t offset);


/*********************
 ** hw/pci/msi(x).h **
 *********************/

int msi_init(struct PCIDevice *dev, uint8_t offset,
             unsigned int nr_vectors, bool msi64bit, bool msi_per_vector_mask);

int msix_init(PCIDevice *dev, unsigned short nentries,
              MemoryRegion *table_bar, uint8_t table_bar_nr,
              unsigned table_offset, MemoryRegion *pba_bar,
              uint8_t pba_bar_nr, unsigned pba_offset, uint8_t cap_pos);

bool msi_enabled(const PCIDevice *dev);
int  msix_enabled(PCIDevice *dev);

int  msix_vector_use(PCIDevice *dev, unsigned vector);
void msix_vector_unuse(PCIDevice *dev, unsigned vector);

void msi_notify(PCIDevice *dev, unsigned int vector);
void msix_notify(PCIDevice *dev, unsigned vector);


/*************************
 ** migration/vmstate.h **
 *************************/

#define VMSTATE_BOOL(...) {}
#define VMSTATE_UINT8(...) {}
#define VMSTATE_UINT32(...) {}
#define VMSTATE_UINT32_TEST(...) {}
#define VMSTATE_UINT64(...) {}
#define VMSTATE_INT32(...) {}
#define VMSTATE_INT64(...) {}
#define VMSTATE_STRUCT_ARRAY_TEST(...) {}
#define VMSTATE_UINT8_ARRAY(...) {}
#define VMSTATE_STRUCT_VARRAY_UINT32(...) {}
#define VMSTATE_PCIE_DEVICE(...) {}
#define VMSTATE_MSIX(...) {}
#define VMSTATE_TIMER_PTR(...) {}
#define VMSTATE_STRUCT(...) {}
#define VMSTATE_END_OF_LIST() {}

typedef struct VMStateField { unsigned dummy; } VMStateField;
typedef struct VMStateDescription
{
	char const  *name;
	int          version_id;
	int          minimum_version_id;
	int         (*post_load)(void *opaque, int version_id);
	VMStateField *fields;
} VMStateDescription;


/**************
 ** assert.h **
 **************/

#define assert(cond) do { \
	if (!(cond)) { \
		q_printf("assertion faied: %s:%d\n", __FILE__, __LINE__); \
		int *d = (int *)0x0; \
		*d = 1; \
	}} while (0)


/************
 ** traces **
 ************/

#define trace_usb_xhci_irq_msix_use(v)
#define trace_usb_xhci_irq_msix_unuse(v)
#define trace_usb_xhci_irq_msix(v)
#define trace_usb_xhci_irq_msi(v)
#define trace_usb_xhci_queue_event(...)
#define trace_usb_xhci_fetch_trb(...)
#define trace_usb_xhci_run(...)
#define trace_usb_xhci_stop(...)
#define trace_usb_xhci_ep_state(...)
#define trace_usb_xhci_ep_enable(...)
#define trace_usb_xhci_ep_disable(...)
#define trace_usb_xhci_ep_stop(...)
#define trace_usb_xhci_ep_reset(...)
#define trace_usb_xhci_ep_set_dequeue(...)
#define trace_usb_xhci_xfer_async(...)
#define trace_usb_xhci_xfer_nak(...)
#define trace_usb_xhci_xfer_success(...)
#define trace_usb_xhci_xfer_error(...)
#define trace_usb_xhci_xfer_start(...)
#define trace_usb_xhci_unimplemented(...)
#define trace_usb_xhci_ep_kick(...)
#define trace_usb_xhci_xfer_retry(...)
#define trace_usb_xhci_slot_enable(...)
#define trace_usb_xhci_slot_disable(...)
#define trace_usb_xhci_slot_address(...)
#define trace_usb_xhci_slot_configure(...)
#define trace_usb_xhci_irq_intx(...)
#define trace_usb_xhci_slot_reset(...)
#define trace_usb_xhci_slot_evaluate(...)
#define trace_usb_xhci_port_notify(...)
#define trace_usb_xhci_port_link(...)
#define trace_usb_xhci_port_reset(...)
#define trace_usb_xhci_reset(...)
#define trace_usb_xhci_cap_read(...)
#define trace_usb_xhci_port_read(...)
#define trace_usb_xhci_port_write(...)
#define trace_usb_xhci_oper_read(...)
#define trace_usb_xhci_oper_write(...)
#define trace_usb_xhci_runtime_read(...)
#define trace_usb_xhci_runtime_write(...)
#define trace_usb_xhci_doorbell_read(...)
#define trace_usb_xhci_doorbell_write(...)
#define trace_usb_xhci_exit(...)
#define trace_usb_port_claim(...)
#define trace_usb_port_release(...)
#define trace_usb_port_attach(...)
#define trace_usb_port_detach(...)
#define trace_usb_packet_state_fault(...)
#define trace_usb_packet_state_change(...)

/***********************
 ** library interface **
 ***********************/

#define type_init(func) void _type_init_##func(void) { func(); }

#define TYPE_USB_HOST_DEVICE "usb-host"

typedef struct USBHostDevice
{
	void *data;
} USBHostDevice;


USBHostDevice *create_usbdevice(void *data);
void remove_usbdevice(USBHostDevice *device);

void usb_host_destroy();
void usb_host_update_devices();


/***********************
 ** monitor/monitor.h **
 ***********************/

void monitor_printf(Monitor *mon, const char *fmt, ...);


#endif /* _INCLUDE__QEMU_EMUL_H_ */
