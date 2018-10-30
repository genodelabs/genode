#include <stdio.h>
#include <string.h>

#include <base/attached_ram_dataspace.h>
#include <libc/component.h>
#include <util/string.h>

#include "memcpy.h"

using Genode::log;

struct Bytewise_test {

	void start()    { log("start bytewise memcpy");    }
	void finished() { log("finished bytewise memcpy"); }

	void copy(void *dst, const void *src, size_t size) {
		bytewise_memcpy(dst, src, size); }
};

struct Genode_cpy_test {

	void start()    { log("start Genode memcpy");    }
	void finished() { log("finished Genode memcpy"); }

	void copy(void *dst, const void *src, size_t size) {
		Genode::memcpy(dst, src, size); }
};

struct Genode_set_test {

	void start()    { log("start Genode memset");    }
	void finished() { log("finished Genode memset"); }

	void copy(void *dst, const void *, size_t size) {
		Genode::memset(dst, 0, size); }
};

struct Libc_cpy_test {

	void start()    { log("start libc memcpy");    }
	void finished() { log("finished libc memcpy"); }

	void copy(void *dst, const void *src, size_t size) {
		memcpy(dst, src, size); }
};

struct Libc_set_test {

	void start()    { log("start libc memset");    }
	void finished() { log("finished libc memset"); }

	void copy(void *dst, const void *, size_t size) {
		memset(dst, 0, size); }
};

void Libc::Component::construct(Libc::Env &env)
{
	log("Memcpy testsuite started");

	memcpy_test<Bytewise_test>();
	memcpy_test<Genode_cpy_test>();
	memcpy_test<Genode_set_test>();
	memcpy_test<Libc_cpy_test>();
	memcpy_test<Libc_set_test>();

	Genode::Attached_ram_dataspace uncached_ds(env.ram(), env.rm(),
	                                           BUF_SIZE, Genode::UNCACHED);

	memcpy_test<Genode_cpy_test>(uncached_ds.local_addr<void>(),
	                             nullptr, BUF_SIZE);
	memcpy_test<Genode_cpy_test>(nullptr, uncached_ds.local_addr<void>(),
	                             BUF_SIZE);

	log("Memcpy testsuite finished");
}
