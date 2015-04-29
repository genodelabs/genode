/*
 * \brief  DDE kit test program
 * \author Christian Helmuth
 * \date   2008-08-15
 */

/*
 * Copyright (C) 2008-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/env.h>
#include <base/printf.h>
#include <base/lock.h>
#include <base/semaphore.h>

#include <timer_session/connection.h>

extern "C" {
#include <dde_kit/dde_kit.h>
#include <dde_kit/lock.h>
#include <dde_kit/semaphore.h>
#include <dde_kit/printf.h>
#include <dde_kit/interrupt.h>
#include <dde_kit/initcall.h>
#include <dde_kit/pgtab.h>
#include <dde_kit/memory.h>
#include <dde_kit/thread.h>
#include <dde_kit/pci.h>
#include <dde_kit/resources.h>
#include <dde_kit/timer.h>
#include <dde_kit/panic.h>
}

/* we directly map 'jiffies' to 'dde_kit_timer_ticks' */
#define jiffies dde_kit_timer_ticks


static Timer::Session_client *timer = 0;


static void test_locks()
{
	PDBG("=== starting lock test ===");

	struct dde_kit_lock *locks[10];

	PDBG("avail() w/o locks: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(locks)/sizeof(*locks); ++i) {
		dde_kit_lock_init(&locks[i]);
		dde_kit_lock_lock(locks[i]);
	}

	PDBG("avail() w/  locks: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(locks)/sizeof(*locks); ++i) {
		dde_kit_lock_unlock(locks[i]);
		dde_kit_lock_deinit(locks[i]);
	}

	PDBG("avail() w/o locks: %zd", Genode::env()->ram_session()->avail());


	for (unsigned i = 0; i < sizeof(locks)/sizeof(*locks); ++i) {
		dde_kit_lock_init(&locks[i]);
		dde_kit_lock_lock(locks[i]);
	}

	PDBG("avail() w/  locks: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(locks)/sizeof(*locks); ++i) {
		dde_kit_lock_unlock(locks[i]);
		dde_kit_lock_deinit(locks[i]);
	}

	PDBG("avail() w/o locks: %zd", Genode::env()->ram_session()->avail());

	PDBG("=== finished lock test ===");
}


static void test_semaphores()
{
	PDBG("=== starting semaphore test ===");

	dde_kit_sem_t *sem[10];

	PDBG("avail() w/o semaphores: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(sem)/sizeof(*sem); ++i) {
		sem[i] = dde_kit_sem_init(1);
		dde_kit_sem_down(sem[i]);
	}

	PDBG("avail() w/  semaphores: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(sem)/sizeof(*sem); ++i) {
		dde_kit_sem_up(sem[i]);
		dde_kit_sem_deinit(sem[i]);
	}

	PDBG("avail() w/o semaphores: %zd", Genode::env()->ram_session()->avail());


	for (unsigned i = 0; i < sizeof(sem)/sizeof(*sem); ++i) {
		sem[i] = dde_kit_sem_init(4);
		dde_kit_sem_down(sem[i]);
		dde_kit_sem_down(sem[i]);
		dde_kit_sem_down(sem[i]);
		dde_kit_sem_down(sem[i]);
	}

	PDBG("avail() w/  semaphores: %zd", Genode::env()->ram_session()->avail());

	for (unsigned i = 0; i < sizeof(sem)/sizeof(*sem); ++i) {
		dde_kit_sem_up(sem[i]);
		dde_kit_sem_deinit(sem[i]);
	}

	PDBG("avail() w/o semaphores: %zd", Genode::env()->ram_session()->avail());

	PDBG("=== finished semaphore test ===");
}


static void test_printf()
{
	PDBG("=== starting printf test ===");
	dde_kit_print("This is a log message.\n");
	dde_kit_print("This is another log message.\n");
	dde_kit_printf("The quick brown fox jumps over the lazy dog.\n");
	dde_kit_printf("The quick brown fox jumps over %s lazy dogs. ", "three");
	dde_kit_printf("The quick brown fox jumps over %s lazy dog.\n", "a huge");
	PDBG("=== finished printf test ===");
}


#include "i8042.h"

static I8042 *i8042;

static void test_interrupt_init(void *priv)
{
	static unsigned cnt = 0;
	PDBG("%d: %s", cnt, (const char *) priv);
	++cnt;
}


static void test_interrupt_handler(void *priv)
{
	static unsigned cnt = 0;
	PDBG("%d: %s", cnt, (const char *) priv);
	++cnt;

	i8042->flush();
}


static void test_interrupt()
{
	PDBG("=== starting interrupt test ===");
	PDBG("Please use keyboard or mouse to trigger interrupt handling!");

	i8042 = new (Genode::env()->heap()) I8042();

	int err0, err1;
	err0 = dde_kit_interrupt_attach(1, 0, test_interrupt_init,
	                                test_interrupt_handler, (void *)"kbd");

	err1 = dde_kit_interrupt_attach(12, 0, test_interrupt_init,
	                                test_interrupt_handler, (void *)"aux");

	enum { DURATION = 2000 };

	if (!err0 && !err1) {
		dde_kit_interrupt_disable(12);
		PDBG("IRQ12 disabled");

		timer->msleep(DURATION);

		dde_kit_interrupt_disable(1);
		PDBG("IRQ1 disabled");

		timer->msleep(DURATION);

		dde_kit_interrupt_enable(12);
		i8042->reset();
		PDBG("IRQ12 enabled");

		timer->msleep(DURATION);

		dde_kit_interrupt_enable(1);
		i8042->reset();
		PDBG("IRQ1 enabled");

		timer->msleep(DURATION);
	} else
		PERR("interrupt attach failed");

	if (!err0) dde_kit_interrupt_detach(1);
	if (!err1) dde_kit_interrupt_detach(12);

	destroy(Genode::env()->heap(), i8042);

	PDBG("=== finished interrupt test ===");
}


static int test_initcall_fn()
{
	PDBG("called");
	return 0;
}

DDE_KIT_INITCALL(test_initcall_fn,noid);
DDE_KIT_INITCALL(test_initcall_fn,id);


static void test_initcall()
{
	PDBG("=== starting initcall test ===");

	dde_kit_initcall_id_test_initcall_fn();
	dde_kit_initcall_noid_test_initcall_fn();

	PDBG("=== finished initcall test ===");
}


static void test_pgtab()
{
	PDBG("=== starting pgtab test ===");

	dde_kit_addr_t virt, phys; dde_kit_size_t size;

	virt = phys = 0;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));

	virt = 0x40000000; phys = 0x20000000; size =  0x10000000;
	dde_kit_pgtab_set_region((void *)virt, phys, size >> DDE_KIT_PAGE_SHIFT);
	PDBG("virt [%lx,%lx) => phys [%lx,%lx)", virt, virt + size, phys, phys + size);

	virt = 0x80000000; phys = 0x80000000; size =  0x10000000;
	dde_kit_pgtab_set_region_with_size((void *)virt, phys, size);
	PDBG("virt [%lx,%lx) => phys [%lx,%lx)", virt, virt + size, phys, phys + size);

	virt = 0x40000000; phys = 0x20000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));
	virt = phys = 0x80000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));

	virt = 0x40000000;
	dde_kit_pgtab_clear_region((void *)virt);

	virt = 0x40000000; phys = 0x20000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));
	virt = phys = 0x80000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));

	virt = 0x80000000;
	dde_kit_pgtab_clear_region((void *)virt);

	virt = 0x40000000; phys = 0x20000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));
	virt = phys = 0x80000000;
	PDBG("phys(%lx) => %lx", virt, dde_kit_pgtab_get_physaddr((void *)virt));
	PDBG("virt(%lx) => %lx", phys, dde_kit_pgtab_get_virtaddr(phys));
	PDBG("size(%lx) => %zx", virt, dde_kit_pgtab_get_size((void *)virt));

	PDBG("=== finished pgtab test ===");
}


static void test_memory()
{
	PDBG("=== starting memory test ===");

	void *b0, *b1; dde_kit_size_t size;

	PDBG("--- large-block memory allocator ---");

	b0 = 0; size = 0x1000;
	b0 = dde_kit_large_malloc(size);
	PDBG("phys(%p) => %lx", b0, dde_kit_pgtab_get_physaddr(b0));
	PDBG("size(%p) => %zx", b0, dde_kit_pgtab_get_size(b0));
	dde_kit_large_free(b0);

	/* must fail */
	PDBG("phys(%p) => %lx", b0, dde_kit_pgtab_get_physaddr(b0));

	b0 = 0; size = 0x3fff;
	b0 = dde_kit_large_malloc(size);
	b1 = 0; size = 0x4000;
	b1 = dde_kit_large_malloc(size);
	PDBG("phys(%p) => %lx", b0, dde_kit_pgtab_get_physaddr(b0));
	PDBG("size(%p) => %zx", b0, dde_kit_pgtab_get_size(b0));
	PDBG("phys(%p) => %lx", b1, dde_kit_pgtab_get_physaddr(b1));
	PDBG("size(%p) => %zx", b1, dde_kit_pgtab_get_size(b1));
	dde_kit_large_free(b0);
	dde_kit_large_free(b1);

	PDBG("--- simple memory allocator ---");

	b0 = 0; size = 32;
	b0 = dde_kit_simple_malloc(size);
	b1 = 0; size = 64;
	b1 = dde_kit_simple_malloc(size);

	/* must fail */
	PDBG("phys(%p) => %lx", b0, dde_kit_pgtab_get_physaddr(b0));

	dde_kit_simple_free(b0);
	dde_kit_simple_free(b1);

	PDBG("--- slab allocator ---");

	struct dde_kit_slab *cache;
	enum { num_ptr = 100 };
	void *ptr[num_ptr];
	Genode::size_t sizes[4] = { 404, 36, 2004, 50 };

	for (unsigned j = 0; j < sizeof(sizes) / sizeof(*sizes); ++j) {

		PDBG("size = %zd", sizes[j]);
		PDBG("avail() w/o slab cache: %zd", Genode::env()->ram_session()->avail());

		cache = dde_kit_slab_init(sizes[j]);

		PDBG("avail() w/  slab cache: %zd", Genode::env()->ram_session()->avail());

		dde_kit_slab_set_data(cache, &cache);

		for (unsigned i = 0; i < num_ptr; ++i) ptr[i] = dde_kit_slab_alloc(cache);

		PDBG("  slab50 phys(%p) => %lx", ptr[50], dde_kit_pgtab_get_physaddr(ptr[50]));
		PDBG("  slab50 size(%p) => %zx", ptr[50], dde_kit_pgtab_get_size(ptr[50]));

		PDBG("avail() w/  slab alloc: %zd", Genode::env()->ram_session()->avail());

		for (unsigned i = 0; i < num_ptr; ++i) dde_kit_slab_free(cache, ptr[i]);

		/* could fail */
		PDBG("  slab50 phys(%p) => %lx", ptr[50], dde_kit_pgtab_get_physaddr(ptr[50]));
		PDBG("  slab50 size(%p) => %zx", ptr[50], dde_kit_pgtab_get_size(ptr[50]));

		if (dde_kit_slab_get_data(cache) != &cache)
			PERR("slab cache data pointer corrupt");
		else
			dde_kit_slab_destroy(cache);

		PDBG("avail() w/o slab cache: %zd", Genode::env()->ram_session()->avail());

		/* must fail */
		PDBG("  slab50 phys(%p) => %lx", ptr[50], dde_kit_pgtab_get_physaddr(ptr[50]));
		PDBG("  slab50 size(%p) => %zx", ptr[50], dde_kit_pgtab_get_size(ptr[50]));
	}

	PDBG("=== finished memory test ===");
}


static Genode::Semaphore *ready;

static void test_thread_fn(void *p)
{
	Genode::Lock *lock = (Genode::Lock *)p;

	for (unsigned i = 0; i < 3; ++i) {
		lock->lock();
		PDBG("Here I am arg=%p name=\"%s\" id=%x data=%p",
		     lock,
		     dde_kit_thread_get_name(dde_kit_thread_myself()),
		     dde_kit_thread_get_id(dde_kit_thread_myself()),
		     dde_kit_thread_get_my_data());

		dde_kit_thread_schedule();

		switch (i) {
		case 0:
			dde_kit_thread_msleep((unsigned long)dde_kit_thread_get_my_data() / 10);
			break;
		case 1:
			dde_kit_thread_usleep((unsigned long)dde_kit_thread_get_my_data() * 100);
			break;
		case 2:
			dde_kit_thread_nsleep((unsigned long)dde_kit_thread_get_my_data() * 100000);
			break;
		}

		dde_kit_thread_set_my_data((void *) 0);
		ready->up();
	}

	lock->lock();
	dde_kit_thread_exit();
}


static void test_thread()
{
	static Genode::Lock lock1(Genode::Lock::LOCKED);
	static Genode::Lock lock2(Genode::Lock::LOCKED);
	static Genode::Lock lock3(Genode::Lock::LOCKED);

	static Genode::Semaphore _ready; ready = &_ready;

	struct dde_kit_thread *t[3];

	PDBG("=== starting thread test ===");

	/* we need timing for msleep() */
	dde_kit_timer_init(0, 0);

	t[0] = dde_kit_thread_create(test_thread_fn, &lock1, "eins");
	t[1] = dde_kit_thread_create(test_thread_fn, &lock2, "zwei");
	t[2] = dde_kit_thread_create(test_thread_fn, &lock3, "drei");

	dde_kit_thread_set_data(t[0], (void *)0x1000);
	dde_kit_thread_set_data(t[1], (void *)0x2000);
	dde_kit_thread_set_data(t[2], (void *)0x3000);

	lock1.unlock(); lock2.unlock(); lock3.unlock();
	_ready.down(); _ready.down(); _ready.down();

	PDBG("t[0]->data = %p", dde_kit_thread_get_data(t[0]));
	PDBG("t[1]->data = %p", dde_kit_thread_get_data(t[1]));
	PDBG("t[2]->data = %p", dde_kit_thread_get_data(t[2]));

	dde_kit_thread_set_data(t[0], (void *)0x1001);
	dde_kit_thread_set_data(t[1], (void *)0x2001);
	dde_kit_thread_set_data(t[2], (void *)0x3001);

	lock1.unlock(); lock2.unlock(); lock3.unlock();
	_ready.down(); _ready.down(); _ready.down();

	PDBG("t[0]->data = %p", dde_kit_thread_get_data(t[0]));
	PDBG("t[1]->data = %p", dde_kit_thread_get_data(t[1]));
	PDBG("t[2]->data = %p", dde_kit_thread_get_data(t[2]));

	dde_kit_thread_set_data(t[0], (void *)0x1002);
	dde_kit_thread_set_data(t[1], (void *)0x2002);
	dde_kit_thread_set_data(t[2], (void *)0x3002);

	lock1.unlock(); lock2.unlock(); lock3.unlock();
	_ready.down(); _ready.down(); _ready.down();

	PDBG("t[0]->data = %p", dde_kit_thread_get_data(t[0]));
	PDBG("t[1]->data = %p", dde_kit_thread_get_data(t[1]));
	PDBG("t[2]->data = %p", dde_kit_thread_get_data(t[2]));

	lock1.unlock(); lock2.unlock(); lock3.unlock();

	struct dde_kit_thread *me = dde_kit_thread_adopt_myself("main");

	dde_kit_thread_set_my_data((void *) 0xf000);
	PDBG("me->data = %p", dde_kit_thread_get_my_data());
	dde_kit_thread_set_data(me, (void *) 0xf001);
	PDBG("me->data = %p", dde_kit_thread_get_data(me));

	PDBG("=== finished thread test ===");
}


static void test_pci()
{
	PDBG("=== starting PCI test ===");

	dde_kit_pci_init(0, 0);

	enum { BUS_MAX = 4, DEV_MAX = 8 };

	PDBG("direct access to bus 0-%d devices 0-%d function 0", BUS_MAX, DEV_MAX);

	for (unsigned b = 0; b < BUS_MAX; ++b)
		for (unsigned d = 0; d < DEV_MAX; ++d) {
			unsigned v;
			dde_kit_pci_readl(b, d, 0, 0, &v);
			PDBG("  PCI %02x:%02x.0: %08x", b, d, v);
		}

	PDBG("iterating PCI bus hierarchy with convenience functions");

	int bus = 0, dev = 0, fun = 0;

	for (int ret = dde_kit_pci_first_device(&bus, &dev, &fun);
	     ret == 0;
	     ret = dde_kit_pci_next_device(&bus, &dev, &fun))
		PDBG("  Found PCI %02x:%02x.%x", bus, dev, fun);

	PDBG("=== finished PCI test ===");
}


static void test_resources()
{
	PDBG("=== starting resource test ===");

	dde_kit_addr_t addr, a; dde_kit_size_t size, s; int wc;
	dde_kit_addr_t vaddr; int ret;

	/* should succeed */
	addr = 0xf0000000; size = 0x01000000; wc = 1;
	ret = dde_kit_request_mem(addr, size, wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p (pgtab %p)", addr, addr + size, ret, (void *)vaddr,
	     (void *)dde_kit_pgtab_get_physaddr((void *)vaddr));
	/* rerequest resource */
	ret = dde_kit_request_mem(addr, size, wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p (pgtab %p)", addr, addr + size, ret, (void *)vaddr,
	     (void *)dde_kit_pgtab_get_physaddr((void *)vaddr));
	/* rerequest part of resource */
	a = addr + 0x2000; s = size - 0x2000;
	ret = dde_kit_request_mem(a, s, wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p", a, a + s, ret, (void *)vaddr);
	/* rerequest resource with different access type which fails */
	a = addr + 0x2000; s = size - 0x2000;
	ret = dde_kit_request_mem(a, s, !wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p", a, a + s, ret, (void *)vaddr);
	/* request resource overlapping existing which fails */
	a = addr + size / 2; s = size;
	ret = dde_kit_request_mem(a, s, wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p", a, a + s, ret, (void *)vaddr);
	/* release resource */
	ret = dde_kit_release_mem(addr, size);
	PDBG("mrel [%04lx,%04lx) => %d (pgtab %p)", addr, addr + size, ret,
	     (void *)dde_kit_pgtab_get_physaddr((void *)vaddr));

	/* should fail */
	addr = 0x1000; size = 0x1000; wc = 0;
	ret = dde_kit_request_mem(addr, size, wc, &vaddr);
	PDBG("mreq [%04lx,%04lx) => %d @ %p", addr, addr + size, ret, (void *)vaddr);
	PDBG("mrel [%04lx,%04lx) => %d", addr, addr + size, dde_kit_release_mem(addr, size));

	PDBG("=== finished resource test ===");
}


static void test_timer_fn(void *id)
{
	PDBG("timer %ld fired at %ld", (long) id, jiffies);
}


static void test_timer()
{
	PDBG("=== starting timer tick test ===");

	dde_kit_timer_init(0, 0);

	PDBG("--- tick ---");

	PDBG("timer tick: %ld (%ld)", jiffies, dde_kit_timer_ticks);

	timer->msleep(2000);

	PDBG("timer tick: %ld (%ld)", jiffies, dde_kit_timer_ticks);

	PDBG("--- simple timer ---");

	dde_kit_timer_add(test_timer_fn, (void *)1, jiffies + 2 * DDE_KIT_HZ);
	dde_kit_timer_add(test_timer_fn, (void *)2, jiffies + 4 * DDE_KIT_HZ);
	dde_kit_timer *t = dde_kit_timer_add(test_timer_fn, (void *)3,
	                                     jiffies + 6 * DDE_KIT_HZ);

	timer->msleep(5000);

	dde_kit_timer_del(t);

	PDBG("timer tick: %ld (%ld)", jiffies, dde_kit_timer_ticks);

	timer->msleep(2000);

	PDBG("--- stress test ---");

	dde_kit_timer *timers[512];
	for (unsigned long i = 0; i < sizeof(timers)/sizeof(*timers); ++i)
		timers[i] = dde_kit_timer_add(test_timer_fn, (void *)i, jiffies + (i % 128) * (DDE_KIT_HZ / 100));

	PDBG("created %zd timers", sizeof(timers)/sizeof(*timers));

	timer->msleep(500);

	for (unsigned i = 0; i < sizeof(timers)/sizeof(*timers); ++i)
		dde_kit_timer_del(timers[i]);

	PDBG("deleted %zd timers", sizeof(timers)/sizeof(*timers));

	timer->msleep(2000);

	PDBG("=== finished timer tick test ===");
}


static void test_panic()
{
	PDBG("=== starting panic test ===");

	if (1)
		dde_kit_panic("Don't panic, it's just a test.");
	else
		dde_kit_debug("Don't panic, it's just a test.");

	PDBG("=== finished panic test ===");
}


int main(int argc, char **argv)
{
	PDBG("test-dde_kit started...");

	timer = new (Genode::env()->heap()) Timer::Connection();

	if (0) test_locks();
	if (0) test_semaphores();
	if (0) test_printf();
	if (0) test_interrupt();
	if (0) test_initcall();
	if (0) test_pgtab();
	if (0) test_memory();
	if (0) test_thread();
	if (1) test_pci();
	if (0) test_resources();
	if (0) test_timer();

	if (1) test_panic();

	return 0;
}
