/*
 * \brief  Qemu USB controller interface
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \date   2015-12-14
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>
#include <base/log.h>
#include <util/misc_math.h>

/* local includes */
#include <extern_c_begin.h>
#include <qemu_emul.h>
/* XHCIState is defined in this file */
#include <hcd-xhci.c>
#include <extern_c_end.h>

#include <qemu/usb.h>

/*******************
 ** USB interface **
 *******************/

static bool const verbose_irq  = false;
static bool const verbose_iov  = false;
static bool const verbose_mmio = false;

extern "C" void _type_init_usb_register_types();
extern "C" void _type_init_usb_host_register_types(Genode::Signal_receiver*);
extern "C" void _type_init_xhci_register_types();

extern Genode::Lock _lock;

Qemu::Controller *qemu_controller();


static Qemu::Timer_queue* _timer_queue;
static Qemu::Pci_device*  _pci_device;


Qemu::Controller *Qemu::usb_init(Timer_queue &tq, Pci_device &pci, Genode::Signal_receiver &sig_rec)
{
	_timer_queue = &tq;
	_pci_device  = &pci;

	_type_init_usb_register_types();
	_type_init_xhci_register_types();
	_type_init_usb_host_register_types(&sig_rec);

	return qemu_controller();
}

void reset_controller();

void Qemu::usb_reset()
{
	usb_host_destroy();

	reset_controller();
}


void Qemu::usb_update_devices() {
	usb_host_update_devices(); }


void Qemu::usb_timer_callback(void (*cb)(void*), void *data)
{
	Genode::Lock::Guard g(_lock);

	cb(data);
}


/**********
 ** libc **
 **********/

void *malloc(size_t size) {
	return Genode::env()->heap()->alloc(size); }


void *memset(void *s, int c, size_t n) {
	return Genode::memset(s, c, n); }


void free(void *p) {
	Genode::env()->heap()->free(p, 0); }


void q_printf(char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	Genode::vprintf(fmt, va);
	va_end(va);
}


int snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(buf, size);
	sc.vprintf(fmt, args);
	va_end(args);

	return sc.len();
}


int strcmp(const char *s1, const char *s2)
{
	return Genode::strcmp(s1, s2);
}


/********************
 ** Qemu emulation **
 ********************/

/**
 * Set properties of objects
 */
template <typename T>
void properties_apply(T *state, DeviceClass *klass)
{
	Property *p = klass->props;

	if (!p)
		return;

	while (p->type != END) {
		char *s = (char *)state;
		s += p->offset;
		uint32_t *member = (uint32_t *)s;
		if (p->type == BIT)
			*member |= p->value;
		if (p->type == UINT32)
			*member = p->value;

		p++;
	}
}


/**
 * Class and objects wrapper
 */
struct Wrapper
{
	unsigned long  _start;
	Object         _object;
	DeviceState    _device_state;
	PCIDevice      _pci_device;
	USBDevice      _usb_device;
	BusState       _bus_state;
	XHCIState     *_xhci_state = nullptr;
	USBHostDevice  _usb_host_device;

	ObjectClass         _object_class;
	DeviceClass         _device_class;
	PCIDeviceClass      _pci_device_class;
	BusClass            _bus_class;
	USBDeviceClass      _usb_device_class;
	HotplugHandlerClass _hotplug_handler_class;
	unsigned long  _end;

	Wrapper() { }

	bool in_object(void * ptr)
	{
		/*
		 * XHCIState is roughly 3 MiB large so we only create on instance
		 * (see below) and have to do this pointer check shenanigans.
		 */
		if (_xhci_state != nullptr
		    && ptr >= _xhci_state
		    && ptr < ((char*)_xhci_state + sizeof(XHCIState)))
			return true;

		using addr_t = Genode::addr_t;

		addr_t p = (addr_t)ptr;

		if (p > (addr_t)&_start && p < (addr_t)&_end)
			return true;

		return false;
	}
};


struct Object_pool
{
	enum {
		XHCI,            /* XHCI controller */
		USB_BUS,         /* bus driver */
		USB_DEVICE,      /* USB device driver */
		USB_HOST_DEVICE, /* USB host device driver */
		MAX = 8          /* host devices (USB_HOST_DEVICE - MAX) */
	};

	bool used[MAX];
	Wrapper obj[MAX];

	Wrapper *create_object()
	{
		for (unsigned i = USB_HOST_DEVICE + 1; i < MAX; i++) {
			if (used[i] == false) {
				used[i] = true;
				return &obj[i];
			}
		}
		return nullptr;
	}

	void free_object(Wrapper *w)
	{
		for (unsigned i = USB_HOST_DEVICE + 1; i < MAX; i++)
			if (w == &obj[i]) {
				used[i] = false;
				break;
			}
	}

	Wrapper * find_object(void *ptr)
	{
		for (Wrapper &w : obj) {
			if(w.in_object(ptr))
				return &w;
		}

		Genode::error("object for pointer not found called from: ",
		              __builtin_return_address(0));
		throw -1;
	}

	XHCIState *xhci_state() {
		return obj[XHCI]._xhci_state; }

	BusState *bus() {
		return &obj[USB_BUS]._bus_state; }

	static Object_pool *p()
	{
		static Object_pool _p;
		return &_p;
	}
};


/***********
 ** casts **
 ***********/

struct PCIDevice *cast_PCIDevice(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_pci_device; }


struct XHCIState *cast_XHCIState(void *ptr) {
	return Object_pool::p()->find_object(ptr)->_xhci_state; }


struct DeviceState *cast_DeviceState(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_device_state; }


struct BusState *cast_BusState(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_bus_state; }


struct USBDevice *cast_USBDevice(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_usb_device; }


struct Object *cast_object(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_object; }


struct PCIDeviceClass *cast_PCIDeviceClass(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_pci_device_class; }


struct DeviceClass *cast_DeviceClass(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_device_class; }


struct USBDeviceClass *cast_USBDeviceClass(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_usb_device_class; }


struct BusClass *cast_BusClass(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_bus_class; }


struct HotplugHandlerClass *cast_HotplugHandlerClass(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_hotplug_handler_class; }


struct USBHostDevice *cast_USBHostDevice(void *ptr) {
	return &Object_pool::p()->find_object(ptr)->_usb_host_device; }


struct USBBus *cast_DeviceStateToUSBBus(void) {
	return &Object_pool::p()->xhci_state()->bus; }


USBHostDevice *create_usbdevice(void *data)
{
	Wrapper *obj = Object_pool::p()->create_object();
	if (!obj) {
		Genode::error("could not create new object");
		return nullptr;
	}

	obj->_usb_device_class = Object_pool::p()->obj[Object_pool::USB_HOST_DEVICE]._usb_device_class;

	/*
	 * Set parent bus object
	 */
	DeviceState *dev_state = &obj->_device_state;
	dev_state->parent_bus  = Object_pool::p()->bus();

	/* set data pointer */
	obj->_usb_host_device.data = data;

	/*
	 * Attach new USB host device to USB device driver
	 */
	Error *e = nullptr;
	DeviceClass *usb_device_class = &Object_pool::p()->obj[Object_pool::USB_DEVICE]._device_class;
	usb_device_class->realize(dev_state, &e);
	if (e) {
		error_free(e);
		e = nullptr;

		usb_device_class->unrealize(dev_state, &e);
		Object_pool::p()->free_object(obj);

		return nullptr;
	}

	return &obj->_usb_host_device;
}


void remove_usbdevice(USBHostDevice *device)
{
	DeviceClass *usb_device_class = &Object_pool::p()->obj[Object_pool::USB_DEVICE]._device_class;
	DeviceState *usb_device_state = cast_DeviceState(device);

	if (usb_device_class == nullptr)
		Genode::error("usb_device_class null");

	if (usb_device_state == nullptr)
		Genode::error("usb_device_class null");

	Error *e = nullptr;
	usb_device_class->unrealize(usb_device_state, &e);

	Wrapper *obj = Object_pool::p()->find_object(device);
	Object_pool::p()->free_object(obj);
}


void reset_controller()
{
	Wrapper *w = &Object_pool::p()->obj[Object_pool::XHCI];
	DeviceClass *dc = &w->_device_class;

	dc->reset(&w->_device_state);
}


/***********************************
 ** Qemu interface implementation **
 ***********************************/

Type type_register_static(TypeInfo const *t)
{
	if (!Genode::strcmp(t->name, TYPE_XHCI)) {
		Wrapper *w = &Object_pool::p()->obj[Object_pool::XHCI];
		w->_xhci_state = (XHCIState*) malloc(sizeof(XHCIState));
		Genode::memset(w->_xhci_state, 0, sizeof(XHCIState));

		t->class_init(&w->_object_class, 0);

		properties_apply(w->_xhci_state, &w->_device_class);

		PCIDevice      *pci       = &w->_pci_device;
		PCIDeviceClass *pci_class = &w->_pci_device_class;
		Error *e;
		pci_class->realize(pci, &e);
	}

	if (!Genode::strcmp(t->name, TYPE_USB_DEVICE)) {
		Wrapper *w = &Object_pool::p()->obj[Object_pool::USB_DEVICE];
		t->class_init(&w->_object_class, 0);
	}

	if (!Genode::strcmp(t->name, TYPE_USB_HOST_DEVICE)) {
		Wrapper *w = &Object_pool::p()->obj[Object_pool::USB_HOST_DEVICE];
		t->class_init(&w->_object_class, 0);
	}

	if (!Genode::strcmp(t->name, TYPE_USB_BUS)) {
		Wrapper *w = &Object_pool::p()->obj[Object_pool::USB_BUS];
		ObjectClass *c = &w->_object_class;
		t->class_init(c, 0);
	}

	return nullptr;
}


void qbus_create_inplace(void* bus, size_t size , const char* type,
                         DeviceState *parent, const char *name)
{
	Wrapper    *w =  &Object_pool::p()->obj[Object_pool::USB_BUS];
	BusState   *b = &w->_bus_state;
	char const *n = "xhci.0";
	b->name = (char *)malloc(Genode::strlen(n) + 1);
	Genode::strncpy(b->name, n, Genode::strlen(n) + 1);
}


void timer_del(QEMUTimer *t)
{
	_timer_queue->deactivate_timer(t);
}


void timer_free(QEMUTimer *t)
{
	_timer_queue->delete_timer(t);
	free(t);
}


void timer_mod(QEMUTimer *t, int64_t expire)
{
	_timer_queue->activate_timer(t, expire);
}


QEMUTimer* timer_new_ns(QEMUClockType, void (*cb)(void*), void *opaque)
{
	QEMUTimer *t = (QEMUTimer*)malloc(sizeof(QEMUTimer));
	if (t == nullptr) {
		Genode::error("could not create QEMUTimer");
		return nullptr;
	}

	_timer_queue->register_timer(t, cb, opaque);

	return t;
}


int64_t qemu_clock_get_ns(QEMUClockType) {
	return _timer_queue->get_ns(); }


struct Controller : public Qemu::Controller
{
	struct Mmio
	{
		Genode::addr_t id;
		Genode::size_t size;
		Genode::off_t  offset;

		MemoryRegionOps const *ops;
	} mmio_regions [16];

	uint64_t _mmio_size;

	typedef Genode::Hex Hex;

	Controller()
	{
		Genode::memset(mmio_regions, 0, sizeof(mmio_regions));
	}

	void   mmio_add_region(uint64_t size) { _mmio_size = size; }

	void   mmio_add_region_io(Genode::addr_t id, uint64_t size, MemoryRegionOps const *ops)
	{
		for (Mmio &mmio : mmio_regions) {
			if (mmio.id != 0) continue;

			mmio.id   = id;
			mmio.size = size;
			mmio.ops  = ops;
			return;
		}
	}

	Mmio &find_region(Genode::off_t offset)
	{
		for (Mmio &mmio : mmio_regions) {
			if (offset >= mmio.offset
			    && (offset < mmio.offset + (Genode::off_t)mmio.size))
				return mmio;
		}

		Genode::error("could not find MMIO region for offset: ",
		              Genode::Hex(offset));
		throw -1;
	}

	void   mmio_add_sub_region(Genode::addr_t id, Genode::off_t offset)
	{
		for (Mmio &mmio : mmio_regions) {
			if (mmio.id != id) continue;

			mmio.offset = offset;
			return;
		}
	}

	size_t mmio_size() const { return _mmio_size; }

	int    mmio_read(Genode::off_t offset, void *buf, size_t size)
	{
		Genode::Lock::Guard g(_lock);
		Mmio &mmio        = find_region(offset);
		Genode::off_t reg = offset - mmio.offset;

		void *ptr = Object_pool::p()->xhci_state();

		/*
		 * Handle port access
		 */
		if (offset >= (OFF_OPER + 0x400) && offset < OFF_RUNTIME) {
			uint32_t port = (offset - 0x440) / 0x10;
			ptr = (void*)&Object_pool::p()->xhci_state()->ports[port];
		}

		uint64_t v        = mmio.ops->read(ptr, reg, size);
		*((uint32_t*)buf) = v;

		if (verbose_mmio)
			Genode::log(__func__, ": ", Hex(mmio.id), " offset: ", Hex(offset), " "
			            "reg: ", Hex(reg), " v: ", Hex(v));

		return 0;
	}

	int    mmio_write(Genode::off_t offset, void const *buf, size_t size)
	{
		Genode::Lock::Guard g(_lock);
		Mmio &mmio        = find_region(offset);
		Genode::off_t reg = offset - mmio.offset;
		uint64_t v        = *((uint64_t*)buf);

		void *ptr = Object_pool::p()->xhci_state();

		/*
		 * Handle port access
		 */
		if (offset >= (OFF_OPER + 0x400) && offset < OFF_RUNTIME) {
			uint32_t port = (offset - 0x440) / 0x10;
			ptr = (void*)&Object_pool::p()->xhci_state()->ports[port];
		}

		mmio.ops->write(ptr, reg, v, size);

		if (verbose_mmio)
			Genode::log(__func__, ": ", Hex(mmio.id), " offset: ", Hex(offset), " "
			            "reg: ", Hex(reg), " v: ", Hex(v));

		return 0;
	}
};


static Controller *controller()
{
	static Controller _inst;
	return &_inst;
}

Qemu::Controller *qemu_controller()
{
	return controller();
}


/**********
 ** MMIO **
 **********/

void memory_region_init(MemoryRegion *mr, Object *obj, const char *name, uint64_t size) {
	controller()->mmio_add_region(size); }


void memory_region_init_io(MemoryRegion* mr, Object* obj, const MemoryRegionOps* ops,
                           void *, const char * name, uint64_t size) {
	controller()->mmio_add_region_io((Genode::addr_t)mr, size, ops); }


void memory_region_add_subregion(MemoryRegion *mr, hwaddr offset, MemoryRegion *sr) {
	controller()->mmio_add_sub_region((Genode::addr_t)sr, offset); }


/*********
 ** DMA **
 *********/

int pci_dma_read(PCIDevice*, dma_addr_t addr, void *buf, dma_addr_t size)
{
	return _pci_device->read_dma(addr, buf, size);
}


int pci_dma_write(PCIDevice*, dma_addr_t addr, const void *buf, dma_addr_t size)
{
	return _pci_device->write_dma(addr, buf, size);
}


int dma_memory_read(AddressSpace*, dma_addr_t addr, void *buf, dma_addr_t size)
{
	return _pci_device->read_dma(addr, buf, size);
}


/****************
 ** Interrupts **
 ****************/

void pci_set_irq(PCIDevice*, int level)
{
	if (verbose_irq)
		Genode::log(__func__, ": IRQ level: ", level);
	_pci_device->raise_interrupt(level);
}


void pci_irq_assert(PCIDevice*)
{
	pci_set_irq(nullptr, 1);
}


int msi_init(PCIDevice *pdev, uint8_t offset, unsigned int nr_vectors, bool msi64bit,
             bool msi_per_vector_mask)
{
	return 0;
}


int msix_init(PCIDevice*, short unsigned int, MemoryRegion*, uint8_t, unsigned int, MemoryRegion*,
              uint8_t, unsigned int, uint8_t)
{
	return 0;
}


bool msi_enabled(const PCIDevice *pdev)
{
	return false;
}


int msix_enabled(PCIDevice *pdev)
{
	return 0;
}


void msi_notify(PCIDevice *pdev, unsigned int level) { }


void msix_notify(PCIDevice *pdev , unsigned int level) { }


/*************************************
 ** IO vector / scatter-gatter list **
 *************************************/

void qemu_iovec_add(QEMUIOVector *qiov, void *base, size_t len)
{
	int niov = qiov->niov;

	if (qiov->alloc_hint <= niov) {
		if (verbose_iov)
			Genode::log(__func__, ": alloc_hint ", qiov->alloc_hint,
			            " <= niov: ", niov);

		qiov->alloc_hint += 64;
		iovec *new_iov = (iovec*) malloc(sizeof(iovec) * qiov->alloc_hint);
		if (new_iov == nullptr) {
			Genode::error("could not reallocate iov");
			throw -1;
		}

		for (int i = 0; i < niov; i++) {
			new_iov[i].iov_base = qiov->iov[i].iov_base;
			new_iov[i].iov_len  = qiov->iov[i].iov_len;
		}

		free(qiov->iov);
		qiov->iov = new_iov;
	}

	if (verbose_iov)
		Genode::log(__func__, ": niov: ", niov, " iov_base: ",
		            &qiov->iov[niov].iov_base, " base: ", base, " len: ", len);

	qiov->iov[niov].iov_base = base;
	qiov->iov[niov].iov_len  = len;
	qiov->size              += len;
	qiov->niov++;
}


void qemu_iovec_destroy(QEMUIOVector *qiov)
{
	qemu_iovec_reset(qiov);

	free(qiov->iov);
	qiov->iov = nullptr;
}


void qemu_iovec_reset(QEMUIOVector *qiov)
{
	qiov->size = 0;
	qiov->niov = 0;
}


void qemu_iovec_init(QEMUIOVector *qiov, int alloc_hint)
{
	if (verbose_iov)
		Genode::log(__func__, " iov: ", qiov->iov, " alloc_hint: ", alloc_hint);

	iovec *iov = qiov->iov;
	if (iov != nullptr) {
		if (alloc_hint > qiov->alloc_hint)
			Genode::error("iov already initialized: ", iov, " and alloc_hint smaller");

		qemu_iovec_reset(qiov);
		return;
	}

	if (alloc_hint <= 0) alloc_hint = 1;

	qiov->alloc_hint = alloc_hint;

	qiov->iov = (iovec*) malloc(sizeof(iovec) * alloc_hint);
	if (qiov->iov == nullptr) {
		Genode::error("could not allocate iov");
		throw -1;
	}

	Genode::memset(qiov->iov, 0, sizeof(iovec) * alloc_hint);

	qemu_iovec_reset(qiov);
}


/* taken from util/iov.c */
size_t iov_from_buf(const struct iovec *iov, unsigned int iov_cnt,
                    size_t offset, const void *buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++) {
		if (offset < iov[i].iov_len) {
			size_t len = Genode::min(iov[i].iov_len - offset, bytes - done);
			memcpy((char*)iov[i].iov_base + offset, (char*)buf + done, len);
			done += len;
			offset = 0;
		} else {
			offset -= iov[i].iov_len;
		}
	}

	assert(offset == 0);
	return done;
}


/* taken from util/iov.c */
size_t iov_to_buf(const struct iovec *iov, const unsigned int iov_cnt,
                  size_t offset, void *buf, size_t bytes)
{
	size_t done;
	unsigned int i;
	for (i = 0, done = 0; (offset || done < bytes) && i < iov_cnt; i++) {
		if (offset < iov[i].iov_len) {
			size_t len = Genode::min(iov[i].iov_len - offset, bytes - done);
			memcpy((char*)buf + done, (char*)iov[i].iov_base + offset, len);
			done += len;
			offset = 0;
		} else {
			offset -= iov[i].iov_len;
		}
	}

	assert(offset == 0);
	return done;
}


void pci_dma_sglist_init(QEMUSGList *sgl, PCIDevice*, int alloc_hint) {
	qemu_iovec_init(sgl, alloc_hint); }


void qemu_sglist_add(QEMUSGList *sgl, dma_addr_t base, dma_addr_t len) {
	qemu_iovec_add(sgl, (void*)base, len); }


void qemu_sglist_destroy(QEMUSGList *sgl) {
	qemu_iovec_destroy(sgl); }


int usb_packet_map(USBPacket *p, QEMUSGList *sgl)
{
	void *mem;

	for (int i = 0; i < sgl->niov; i++) {
		dma_addr_t base = (dma_addr_t) sgl->iov[i].iov_base;
		dma_addr_t len = sgl->iov[i].iov_len;

		while (len) {
			dma_addr_t xlen = len;
			mem = _pci_device->map_dma(base, xlen);
			if (verbose_iov)
				Genode::log("mem: ", mem, " base: ", (void *)base, " len: ",
				            Genode::Hex(len));

			if (!mem) {
				goto err;
			}
			if (xlen > len) {
				xlen = len;
			}
			qemu_iovec_add(&p->iov, mem, xlen);
			len -= xlen;
			base += xlen;
		}
	}
	return 0;

err:
	Genode::error("could not map dma");
	usb_packet_unmap(p, sgl);
	return -1;
}


void usb_packet_unmap(USBPacket *p, QEMUSGList *sgl)
{
	for (int i = 0; i < p->iov.niov; i++) {
		_pci_device->unmap_dma(p->iov.iov[i].iov_base,
		                       p->iov.iov[i].iov_len);
	}
}


/******************
 ** qapi/error.h **
 ******************/

void error_setg(Error **errp, const char *fmt, ...)
{
	assert(*errp == nullptr);

	*errp = (Error*) malloc(sizeof(Error));
	if (*errp == nullptr) {
		Genode::error("could not allocate Error");
		return;
	}

	Error *e = *errp;

	va_list args;

	va_start(args, fmt);
	Genode::String_console sc(e->string, sizeof(e->string));
	sc.vprintf(fmt, args);
	va_end(args);
}


void error_propagate(Error **dst_errp, Error *local_err) {
	*dst_errp = local_err; }


void error_free(Error *err) { free(err); }
