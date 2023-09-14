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

void qemu_printf(char const *, ...) __attribute__((format(printf, 1, 2)));

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
typedef __WCHAR_TYPE__ wchar_t;
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

#define fprintf(fd, fmt, ...) qemu_printf(fmt, ##__VA_ARGS__)
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
#define QEMU_PACKED __attribute__((packed))

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

struct Aml { unsigned dummy; };
typedef struct Aml Aml;


/*****************
 ** compiler.h> **
 *****************/

#define typeof_field(type, field) typeof(((type *)0)->field)
#define type_check(t1,t2) ((t1*)0 - (t2*)0)

/******************
 ** qapi-types.h **
 ******************/


typedef enum OnOffAuto {
	ON_OFF_AUTO_AUTO = 0,
	ON_OFF_AUTO_ON = 1,
	ON_OFF_AUTO_OFF = 2,
	ON_OFF_AUTO__MAX = 3,
} OnOffAuto;


/******************
 ** qapi/error.h **
 ******************/

typedef struct Error { char string[256]; } Error;

void error_setg(Error **errp, const char *fmt, ...);

const char *error_get_pretty(Error *err);
void error_report(const char *fmt, ...);
void error_reportf_err(Error *err, const char *fmt, ...);
void error_free(Error *err);
void error_propagate(Error **dst_errp, Error *local_err);
void error_append_hint(Error *const *errp, const char *fmt, ...);

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

typedef struct ObjectProperty { unsigned dummy; } ObjectProperty;
ObjectProperty *object_property_add_bool(Object *obj, const char *name,
                                         bool (*get)(Object *, Error **),
                                         void (*set)(Object *, bool, Error **));

#define object_initialize_child(parent, propname, child, type)          \
	object_initialize_child_internal((parent), (propname),              \
	                                 (child), sizeof(*(child)), (type))
void object_initialize_child_internal(Object *parent, const char *propname,
                                      void *child, size_t size,
                                      const char *type);

bool object_property_set_link(Object *obj, const char *name,
                              Object *value, Error **errp);

struct DeviceState;
struct PCIDevice;
struct XHCIState;

struct PCIDevice    *cast_PCIDevice(void *);
struct XHCIState    *cast_XHCIState(void *);
struct XHCIPciState *cast_XHCIPciState(void *);
struct DeviceState  *cast_DeviceState(void *);
struct BusState     *cast_BusState(void *);
struct Object       *cast_object(void *);
struct USBDevice    *cast_USBDevice(void *);
struct USBHostDevice *cast_USBHostDevice(void *);

struct PCIDeviceClass      *cast_PCIDeviceClass(void *);
struct DeviceClass         *cast_DeviceClass(void *);
struct BusClass            *cast_BusClass(void *);
struct HotplugHandlerClass *cast_HotplugHandlerClass(void *);

struct USBDeviceClass      *cast_USBDeviceClass(void *);
struct USBWebcamState;
struct USBWebcamState      *cast_USBWebcamState(void *);

struct USBBus *cast_USBBus(void *);

#define OBJECT_CHECK(type, obj, str) \
	cast_##type((void *)obj)

#define OBJECT_CLASS_CHECK(type, klass, str) \
	OBJECT_CHECK(type, (klass), str)

#define OBJECT(obj) \
	cast_object(obj)

#define OBJECT_GET_CLASS(klass, obj, str) \
	OBJECT_CHECK(klass, obj, str)

#define BUS_GET_CLASS(obj) OBJECT_GET_CLASS(BusClass, (obj), TYPE_BUS)

#define DECLARE_INSTANCE_CHECKER(InstanceType, OBJ_NAME, TYPENAME) \
	static inline InstanceType * \
	OBJ_NAME(const void *obj) \
	{ return OBJECT_CHECK(InstanceType, obj, TYPENAME); }

#define DECLARE_CLASS_CHECKERS(ClassType, OBJ_NAME, TYPENAME) \
	static inline ClassType * \
	OBJ_NAME##_GET_CLASS(const void *obj) \
	{ return OBJECT_GET_CLASS(ClassType, obj, TYPENAME); } \
	\
	static inline ClassType * \
	OBJ_NAME##_CLASS(const void *klass) \
	{ return OBJECT_CLASS_CHECK(ClassType, klass, TYPENAME); }

#define DECLARE_OBJ_CHECKERS(InstanceType, ClassType, OBJ_NAME, TYPENAME) \
	DECLARE_INSTANCE_CHECKER(InstanceType, OBJ_NAME, TYPENAME) \
	\
	DECLARE_CLASS_CHECKERS(ClassType, OBJ_NAME, TYPENAME)


#define OBJECT_DECLARE_TYPE(InstanceType, ClassType, MODULE_OBJ_NAME) \
	typedef struct InstanceType InstanceType; \
	typedef struct ClassType ClassType; \
	\
	DECLARE_OBJ_CHECKERS(InstanceType, ClassType, \
                         MODULE_OBJ_NAME, TYPE_##MODULE_OBJ_NAME)


#define OBJECT_DECLARE_SIMPLE_TYPE(InstanceType, MODULE_OBJ_NAME) \
	typedef struct InstanceType InstanceType; \
	\
	DECLARE_INSTANCE_CHECKER(InstanceType, MODULE_OBJ_NAME, TYPE_##MODULE_OBJ_NAME)

void object_unref(void *obj);


typedef struct InterfaceInfo {
	const char *type;
} InterfaceInfo;


typedef struct TypeInfo
{
	char const *name;
	char const *parent;
	size_t      instance_size;

	void (*instance_init)(Object *obj);

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

typedef struct Visitor Visitor;

#define TYPE_DEVICE "device"
OBJECT_DECLARE_TYPE(DeviceState, DeviceClass, DEVICE)


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

#define g_new g_new0

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
	char const *id;
	BusState *parent_bus;
} DeviceState;

struct VMStateDescription;
struct Property;

typedef void (*DeviceRealize)(DeviceState *dev, Error **errp);
typedef void (*DeviceUnrealize)(DeviceState *dev);

typedef struct DeviceClass
{
	DECLARE_BITMAP(categories, DEVICE_CATEGORY_MAX);
	struct Property *props;
	bool user_creatable;
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

struct PropertyInfo { unsigned dummy; };
typedef struct PropertyInfo PropertyInfo;

extern const PropertyInfo qdev_prop_link;
extern const PropertyInfo qdev_prop_on_off_auto;

typedef struct Property
{
	const char   *name;
	const PropertyInfo *info;
	enum Prop_type type;
	bool         set_default;
	union {
		int64_t i;
		uint64_t u;
	} defval;
	unsigned       offset;
	unsigned long  value;
	const char   *link_type;
} Property;


#define DEFINE_PROP_BIT(_name, _state, _field, _bit, _bool) \
{ .type = BIT, .offset = offsetof(_state, _field), .value = (_bool << _bit) }
#define DEFINE_PROP_UINT32(_name, _state, _field, _value) \
{ .type = UINT32, .offset = offsetof(_state, _field), .value = _value }
#define DEFINE_PROP_END_OF_LIST() { .type = END }
#define DEFINE_PROP_STRING(...) {}

#define DEFINE_PROP_SIGNED(_name, _state, _field, _defval, _prop, _type) { \
	.name      = (_name),                                           \
	.info      = &(_prop),                                          \
	.offset    = offsetof(_state, _field)                           \
	    + type_check(_type,typeof_field(_state, _field)),           \
	.set_default = true,                                            \
	.defval.i  = (_type)_defval,                                    \
	}
#define DEFINE_PROP_ON_OFF_AUTO(_n, _s, _f, _d) \
	    DEFINE_PROP_SIGNED(_n, _s, _f, _d, qdev_prop_on_off_auto, OnOffAuto)

#define DEFINE_PROP_LINK(_name, _state, _field, _type, _ptr_type) {     \
	.name = (_name),                                                \
	.info = &(qdev_prop_link),                                      \
	.offset = offsetof(_state, _field)                              \
	    + type_check(_ptr_type, typeof_field(_state, _field)),      \
	.link_type  = _type,                                            \
	}

void device_class_set_props(DeviceClass *dc, Property *props);

void device_legacy_reset(DeviceState *dev);

/* forward */
typedef struct DeviceState DeviceState;
typedef struct HotplugHandler HotplugHandler;

void qdev_simple_device_unplug_cb(HotplugHandler *hotplug_dev,
                                  DeviceState *dev, Error **errp);
void qbus_create_inplace(void *bus, size_t size, const char *typename,
                         DeviceState *parent, const char *name);
void qbus_set_bus_hotplug_handler(BusState *bus);
DeviceState *qdev_create(BusState *bus, const char *name);
DeviceState *qdev_try_create(BusState *bus, const char *name);
char *qdev_get_dev_path(DeviceState *dev);
const char *qdev_fw_name(DeviceState *dev);
DeviceState *qdev_new(const char *name);
DeviceState *qdev_try_new(const char *name);
bool qdev_realize_and_unref(DeviceState *dev, BusState *bus, Error **errp);

void qdev_alias_all_properties(DeviceState *target, Object *source);

extern bool qdev_hotplug;

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
#define MAX_CONST(a, b) MAX((a), (b))
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

void qemu_sglist_init(QEMUSGList *qsg, DeviceState *dev, int alloc_hint, AddressSpace *as);
void qemu_sglist_add(QEMUSGList *qsg, dma_addr_t base, dma_addr_t len);
void qemu_sglist_destroy(QEMUSGList *qsg);

int dma_memory_read(AddressSpace *as, dma_addr_t addr, void *buf, dma_addr_t len);
int dma_memory_write(AddressSpace *as, dma_addr_t addr, const void *buf, dma_addr_t len);

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

	uint32_t cap_present;

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
PCIBus *pci_get_bus(const PCIDevice *dev);

AddressSpace *pci_get_address_space(PCIDevice *dev);

int pcie_endpoint_cap_init(PCIDevice *dev, uint8_t offset);

#define INTERFACE_PCIE_DEVICE "pci-express-device"
#define INTERFACE_CONVENTIONAL_PCI_DEVICE "conventional-pci-device"

#define PCI_VENDOR_ID_REDHAT             0x1b36
#define PCI_DEVICE_ID_REDHAT_XHCI        0x000d

enum {
	QEMU_PCI_CAP_EXPRESS = 0x4,
};

/*********************
 ** hw/pci/msi(x).h **
 *********************/

int msi_init(struct PCIDevice *dev, uint8_t offset,
             unsigned int nr_vectors, bool msi64bit, bool msi_per_vector_mask, Error **errp);

int msix_init(PCIDevice *dev, unsigned short nentries,
              MemoryRegion *table_bar, uint8_t table_bar_nr,
              unsigned table_offset, MemoryRegion *pba_bar,
              uint8_t pba_bar_nr, unsigned pba_offset, uint8_t cap_pos, Error **);
void msix_uninit(PCIDevice *dev, MemoryRegion *table_bar, MemoryRegion *pba_bar);

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
#define VMSTATE_PCI_DEVICE(...) {}
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
		qemu_printf("assertion failed: %s:%d\n", __FILE__, __LINE__); \
		int *d = (int *)0x0; \
		*d = 1; \
	}} while (0)


/************
 ** traces **
 ************/

#if 0
#define TRACE_PRINTF(...)  qemu_printf(__VA_ARGS__)
#else
#define TRACE_PRINTF(...)
#endif

#define trace_usb_packet_state_change(...) TRACE_PRINTF("%s:%d\n", "trace_usb_packet_state_change", __LINE__)
#define trace_usb_packet_state_fault(...)  TRACE_PRINTF("%s:%d\n", "trace_usb_packet_state_fault", __LINE__)
#define trace_usb_port_attach(...)         TRACE_PRINTF("%s:%d\n", "trace_usb_port_attach", __LINE__)
#define trace_usb_port_claim(...)          TRACE_PRINTF("%s:%d\n", "trace_usb_port_claim", __LINE__)
#define trace_usb_port_detach(...)         TRACE_PRINTF("%s:%d\n", "trace_usb_port_detach", __LINE__)
#define trace_usb_port_release(...)        TRACE_PRINTF("%s:%d\n", "trace_usb_port_release", __LINE__)
#define trace_usb_xhci_cap_read(...)       TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_cap_read", __LINE__)
#define trace_usb_xhci_doorbell_read(...)  TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_doorbell_read", __LINE__)
#define trace_usb_xhci_doorbell_write(...) TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_doorbell_write", __LINE__)
#define trace_usb_xhci_enforced_limit(...) TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_enforced_limit", __LINE__)
#define trace_usb_xhci_ep_disable(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_disable", __LINE__)
#define trace_usb_xhci_ep_enable(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_enable", __LINE__)
#define trace_usb_xhci_ep_kick(...)        TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_kick", __LINE__)
#define trace_usb_xhci_ep_reset(...)       TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_reset", __LINE__)
#define trace_usb_xhci_ep_set_dequeue(...) TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_set_dequeue", __LINE__)
#define trace_usb_xhci_ep_state(...)       TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_state", __LINE__)
#define trace_usb_xhci_ep_stop(...)        TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_ep_stop", __LINE__)
#define trace_usb_xhci_exit(...)           TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_exit", __LINE__)
#define trace_usb_xhci_fetch_trb(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_fetch_trb", __LINE__)
#define trace_usb_xhci_irq_intx(...)       TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_irq_intx", __LINE__)
#define trace_usb_xhci_irq_msi(v)
#define trace_usb_xhci_irq_msix(v)
#define trace_usb_xhci_irq_msix_unuse(v)
#define trace_usb_xhci_irq_msix_use(v)
#define trace_usb_xhci_oper_read(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_oper_read", __LINE__)
#define trace_usb_xhci_oper_write(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_oper_write", __LINE__)
#define trace_usb_xhci_port_link(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_port_link", __LINE__)
#define trace_usb_xhci_port_notify(...)    TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_port_notify", __LINE__)
#define trace_usb_xhci_port_read(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_port_read", __LINE__)
#define trace_usb_xhci_port_reset(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_port_reset", __LINE__)
#define trace_usb_xhci_port_write(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_port_write", __LINE__)
#define trace_usb_xhci_queue_event(...)    TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_queue_event", __LINE__)
#define trace_usb_xhci_reset(...)          TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_reset", __LINE__)
#define trace_usb_xhci_run(...)            TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_run", __LINE__)
#define trace_usb_xhci_runtime_read(...)   TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_runtime_read", __LINE__)
#define trace_usb_xhci_runtime_write(...)  TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_runtime_write", __LINE__)
#define trace_usb_xhci_slot_address(...)   TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_address", __LINE__)
#define trace_usb_xhci_slot_configure(...) TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_configure", __LINE__)
#define trace_usb_xhci_slot_disable(...)   TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_disable", __LINE__)
#define trace_usb_xhci_slot_enable(...)    TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_enable", __LINE__)
#define trace_usb_xhci_slot_evaluate(...)  TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_evaluate", __LINE__)
#define trace_usb_xhci_slot_reset(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_slot_reset", __LINE__)
#define trace_usb_xhci_stop(...)           TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_stop", __LINE__)
#define trace_usb_xhci_unimplemented(...)  TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_unimplemented", __LINE__)
#define trace_usb_xhci_xfer_async(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_async", __LINE__)
#define trace_usb_xhci_xfer_error(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_error", __LINE__)
#define trace_usb_xhci_xfer_nak(...)       TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_nak", __LINE__)
#define trace_usb_xhci_xfer_retry(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_retry", __LINE__)
#define trace_usb_xhci_xfer_start(...)     TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_start", __LINE__)
#define trace_usb_xhci_xfer_success(...)   TRACE_PRINTF("%s:%d\n", "trace_usb_xhci_xfer_success", __LINE__)

#define trace_usb_desc_device(...)             TRACE_PRINTF("%s:%d\n", "trace_usb_desc_device", __LINE__)
#define trace_usb_desc_config(...)             TRACE_PRINTF("%s:%d\n", "trace_usb_desc_config", __LINE__)
#define trace_usb_desc_string(...)             TRACE_PRINTF("%s:%d\n", "trace_usb_desc_string", __LINE__)
#define trace_usb_desc_device_qualifier(...)   TRACE_PRINTF("%s:%d\n", "trace_usb_desc_device_qualifier", __LINE__)
#define trace_usb_desc_other_speed_config(...) TRACE_PRINTF("%s:%d\n", "trace_usb_desc_other_speed_config", __LINE__)
#define trace_usb_desc_bos(...)                TRACE_PRINTF("%s:%d\n", "trace_usb_desc_bos", __LINE__)
#define trace_usb_set_addr(...)                TRACE_PRINTF("%s:%d\n", "trace_usb_set_addr", __LINE__)
#define trace_usb_set_config(...)              TRACE_PRINTF("%s:%d\n", "trace_usb_set_config", __LINE__)
#define trace_usb_clear_device_feature(...)    TRACE_PRINTF("%s:%d\n", "trace_usb_clear_device_feature", __LINE__)
#define trace_usb_set_device_feature(...)      TRACE_PRINTF("%s:%d\n", "trace_usb_set_device_feature", __LINE__)
#define trace_usb_set_interface(...)           TRACE_PRINTF("%s:%d\n", "trace_usb_set_interface", __LINE__)
#define trace_usb_desc_msos(...)               TRACE_PRINTF("%s:%d\n", "trace_usb_desc_msos", __LINE__)

/***********************
 ** library interface **
 ***********************/

#define type_init(func) void _type_init_##func(void) { func(); }

#define TYPE_USB_HOST_DEVICE "usb-host"

typedef struct USBHostDevice
{
	void *data;
} USBHostDevice;


USBHostDevice *create_usbdevice(void *data, int speed);
void remove_usbdevice(USBHostDevice *device);

void usb_host_destroy();
void usb_host_update_devices();


/***********************
 ** monitor/monitor.h **
 ***********************/

void monitor_printf(Monitor *mon, const char *fmt, ...);


/*************
 ** errno.h **
 *************/

enum {
	ENOTSUP = 666,
};


#endif /* _INCLUDE__QEMU_EMUL_H_ */
