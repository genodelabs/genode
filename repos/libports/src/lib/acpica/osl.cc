/*
 * \brief  OS specific backend for ACPICA library
 * \author Alexander Boettcher
 * \date   2016-11-14
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>
#include <util/misc_math.h>

#include <io_port_session/connection.h>
#include <timer_session/connection.h>

#include <acpica/acpica.h>

#include "env.h"

extern "C" {
#include "acpi.h"
#include "acpiosxf.h"
}


#define FAIL(retval) \
	{ \
		Genode::error(__func__, ":", __LINE__, " called - dead"); \
		Genode::Lock lock; \
		while (1) lock.lock(); \
		return retval; \
	}

ACPI_STATUS AcpiOsPredefinedOverride (const ACPI_PREDEFINED_NAMES *pre,
                                      ACPI_STRING *newobj)
{
	*newobj = nullptr;
	return AE_OK;
}


void * AcpiOsAllocate (ACPI_SIZE size) { return Acpica::heap().alloc(size); }


void AcpiOsFree (void *ptr)
{
	if (Acpica::heap().need_size_for_free())
		Genode::warning(__func__, " called - warning - ptr=", ptr);

	Acpica::heap().free(ptr, 0);
}

ACPI_STATUS AcpiOsCreateLock (ACPI_SPINLOCK *spin_lock)
{
	*spin_lock = new (Acpica::heap()) Genode::Lock();
	return AE_OK;
}

ACPI_CPU_FLAGS AcpiOsAcquireLock (ACPI_SPINLOCK h)
{
	Genode::Lock *lock = reinterpret_cast<Genode::Lock *>(h);
	lock->lock();

	return AE_OK;
}

void AcpiOsReleaseLock (ACPI_SPINLOCK h, ACPI_CPU_FLAGS flags)
{
	Genode::Lock *lock = reinterpret_cast<Genode::Lock *>(h);

	if (flags != AE_OK)
		Genode::warning("warning - unknown flags in ", __func__);

	lock->unlock();

	return;
}

ACPI_STATUS AcpiOsCreateSemaphore (UINT32 max, UINT32 initial,
                                   ACPI_SEMAPHORE *sem)
{
	*sem = new (Acpica::heap()) Genode::Semaphore(initial);
	return AE_OK;
}

ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE h, UINT32 units,
                                UINT16 timeout_ms)
{
	Genode::Semaphore *sem = reinterpret_cast<Genode::Semaphore *>(h);

	if (!units)
		FAIL(AE_BAD_PARAMETER)

	/**
	 * Timeouts not supported yet ...
	 * == 0     means - try and don't block - we're single threaded - ignore
	 * == 0xfff means - wait endless - fine
	 */
	if (0 < timeout_ms && timeout_ms < 0xffff)
		FAIL(AE_BAD_PARAMETER)

	/* timeout == forever case */
	while (units) {
		sem->down();
		units--;
	}

	return AE_OK;
}

ACPI_STATUS AcpiOsSignalSemaphore (ACPI_SEMAPHORE h, UINT32 units)
{
	Genode::Semaphore *sem = reinterpret_cast<Genode::Semaphore *>(h);

	while (units) {
		sem->up();
		units--;
	}

	return AE_OK;
}

ACPI_STATUS AcpiOsDeleteSemaphore (ACPI_SEMAPHORE)
	FAIL(AE_BAD_PARAMETER)

ACPI_THREAD_ID AcpiOsGetThreadId (void) {
	return reinterpret_cast<Genode::addr_t>(Genode::Thread::myself()); }

ACPI_STATUS AcpiOsTableOverride (ACPI_TABLE_HEADER *x, ACPI_TABLE_HEADER **y)
{
	*y = nullptr;
	return AE_OK;
}

ACPI_STATUS AcpiOsPhysicalTableOverride (ACPI_TABLE_HEADER *x,
                                         ACPI_PHYSICAL_ADDRESS *y, UINT32 *z)
{
	*y = 0;
	return AE_OK;
}

ACPI_STATUS AcpiOsReadPort (ACPI_IO_ADDRESS port, UINT32 *value, UINT32 width)
{
	if (width % 8 != 0)
		FAIL(AE_BAD_PARAMETER)

	/* the I/O port may be owned by drivers, which will cause exceptions */
	try {
		unsigned const bytes = width / 8;
		Genode::Io_port_connection io_port(Acpica::env(), port, bytes);

		switch (bytes) {
		case 1 :
			*value = io_port.inb(port);
			break;
		case 2 :
			*value = io_port.inw(port);
			break;
		case 4 :
			*value = io_port.inl(port);
			break;
		default:
			FAIL(AE_BAD_PARAMETER)
		}
	}
	catch (Genode::Service_denied) { return AE_BAD_PARAMETER; }

	return AE_OK;
}

ACPI_STATUS AcpiOsWritePort (ACPI_IO_ADDRESS port, UINT32 value, UINT32 width)
{
	if (width % 8 != 0)
		FAIL(AE_BAD_PARAMETER)

	/* the I/O port may be owned by drivers, which will cause exceptions */
	try {
		unsigned const bytes = width / 8;
		Genode::Io_port_connection io_port(Acpica::env(), port, bytes);

		switch (bytes) {
		case 1 :
			io_port.outb(port, value);
			break;
		case 2 :
			io_port.outw(port, value);
			break;
		case 4 :
			io_port.outl(port, value);
			break;
		default:
			FAIL(AE_BAD_PARAMETER)
		}
	}
	catch (Genode::Service_denied) { return AE_BAD_PARAMETER; }

	return AE_OK;
}

static struct
{
	ACPI_EXECUTE_TYPE type;
	ACPI_OSD_EXEC_CALLBACK func;
	void *context;
} deferred[8];

ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE type, ACPI_OSD_EXEC_CALLBACK func,
                         void *context)
{
	if (type == OSL_GPE_HANDLER) {
		func(context);
		return AE_OK;
	}

	if (type != OSL_NOTIFY_HANDLER)
		FAIL(AE_BAD_PARAMETER)

	for (unsigned i = 0; i < sizeof(deferred) / sizeof (deferred[0]); i++) {
		if (deferred[i].func)
			continue;

		deferred[i].type = type;
		deferred[i].func = func;
		deferred[i].context = context;
		return AE_OK;
	}
	Genode::error("queue full for deferred handlers");
	return AE_BAD_PARAMETER;
}

void AcpiOsWaitEventsComplete()
{
	for (unsigned i = 0; i < sizeof(deferred) / sizeof (deferred[0]); i++) {
		if (!deferred[i].func)
			continue;

		ACPI_OSD_EXEC_CALLBACK func = deferred[i].func;
		deferred[i].func = nullptr;

		func(deferred[i].context);
	}
}

void AcpiOsSleep (UINT64 sleep_ms)
{
	Genode::log(__func__, " ", sleep_ms, " ms");

	static Timer::Connection conn(Acpica::env());
	conn.msleep(sleep_ms);
	return;
}


/********************************
 * unsupported/unused functions *
 ********************************/

ACPI_STATUS AcpiOsSignal (UINT32, void *)
	FAIL(AE_BAD_PARAMETER)

UINT64 AcpiOsGetTimer (void)
	FAIL(0)

void AcpiOsStall (UINT32)
	FAIL()

ACPI_STATUS AcpiOsReadMemory (ACPI_PHYSICAL_ADDRESS, UINT64 *, UINT32)
	FAIL(AE_BAD_PARAMETER)

ACPI_STATUS AcpiOsWriteMemory (ACPI_PHYSICAL_ADDRESS, UINT64, UINT32)
	FAIL(AE_BAD_PARAMETER)

ACPI_STATUS AcpiOsRemoveInterruptHandler (UINT32, ACPI_OSD_HANDLER)
	FAIL(AE_BAD_PARAMETER)

ACPI_STATUS AcpiOsGetLine (char *, UINT32, UINT32 *)
	FAIL(AE_BAD_PARAMETER)

extern "C"
{
void AcpiAhMatchUuid() FAIL()
void AcpiAhMatchHardwareId() FAIL()
void AcpiDbCommandDispatch() FAIL()
void AcpiDbSetOutputDestination() FAIL()
void MpSaveSerialInfo() FAIL()
void MpSaveGpioInfo() FAIL()
}
