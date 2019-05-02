/*
 * \brief  Test for cache performance
 * \author Johannes Schlatow
 * \date   2019-05-02
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/component.h>
#include <base/heap.h>
#include <trace/timestamp.h>

using namespace Genode::Trace;

class Test
{
	private:
		Genode::Heap      &_alloc;
		Genode::size_t     _size;

		unsigned *_data { 0 };

		Timestamp ts_to_time(Timestamp start, Timestamp end)
		{
			Timestamp diff = end - start;
			if (end < start) diff--;

			return diff;
		}

		Test(const Test &);
		void operator=(const Test&);

	public:

		Test(Genode::Heap &alloc, Genode::size_t size_bytes)
			: _alloc(alloc),
			  _size(size_bytes/sizeof(unsigned))
		{
			_data = new (_alloc) unsigned[_size];
		}

		~Test()
		{
			destroy(_alloc, _data);
		}

		Timestamp read_write(unsigned iterations=100)
		{
			Timestamp start_ts = timestamp();

			for (Genode::size_t i=0; i < iterations; i++) {
				for (Genode::size_t index=0; index < _size; index++) {
					_data[index]++;
				}
			}

			return ts_to_time(start_ts, timestamp()) / iterations;
		}
};

struct Main
{
	Genode::Env &env;

	Genode::Heap heap { env.ram(), env.rm() };

	Main(Genode::Env &env);
};


Main::Main(Genode::Env &env) : env(env)
{
	using namespace Genode;

	log("--- test-cache started ---");

	enum {
		START_SIZE = 8,
		END_SIZE   = 1024 * 4,
		THRESHOLD_PERCENT = 10,
	};

	size_t size = START_SIZE;
	while (size <= END_SIZE)
	{
		log("\n--- Running tests for size ", size, "KB ---");

		Test test(heap, size*1024);
		log("Read/write: ", test.read_write() / size, " cycles on average per KB");

		size = size << 1;
	}

	log("--- test-cache done ---");
}

void Component::construct(Genode::Env &env) { static Main inst(env); }
Genode::size_t Component::stack_size() { return 32*1024*sizeof(long); }

