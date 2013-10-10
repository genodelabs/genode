/*
 * \brief  Simple Iso9660 test program
 * \author Sebastian Sumpf
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/printf.h>
#include <rom_session/connection.h>
#include <dataspace/client.h>
#include <rm_session/connection.h>
#include <base/thread.h>

using namespace Genode;


void dump(uint8_t *ptr, unsigned long offset)
{
	unsigned long *_ptr = (unsigned long *)(ptr + offset);
	for (int i = 0; i < 4; i++) {
		printf("%08lx: ", offset);

		int j;
		for (j = 0; j < 5; j++)
			printf("%08lx  ", _ptr[(i * 8) + j]);

		offset += j * sizeof(unsigned long);
		printf("\n");
	}

	printf("\n");
}


class Pager : public Thread<8192>
{
	private:

		Signal_receiver _receiver;
		Dataspace_capability _ds;
		Rm_connection *_rm;

	public:

		Pager() : Thread("pager") { }

		Signal_receiver *signal_receiver() { return &_receiver; }

		void entry()
		{
			while (true) {
				Signal signal = _receiver.wait_for_signal();

				for (unsigned i = 0; i < signal.num(); i++)
					handle_fault();
			}
		}

		void handle_fault()
		{
			Rm_session::State state = _rm->state();
			addr_t addr = state.addr & ~(0xfff);
			_rm->attach_at(_ds, addr, 0x1000, addr - 0x1000);
		}

		void dataspace(Dataspace_capability ds) { _ds = ds; }
		void rm(Rm_connection *rm) { _rm = rm; }

		static Pager* pager()
		{
			static Pager _pager;
			return &_pager;
		}

		static Pager* pager2()
		{
			static Pager _pager;
			return &_pager;
		}

		static Pager* pager3()
		{
			static Pager _pager;
			return &_pager;
		}
};


int main()
{
	Dataspace_capability ds;
	uint8_t *ptr = 0;
	static Signal_context context;
	static Signal_context context2;

	try {

		Rom_connection rom("/test.txt");
		rom.on_destruction(Rom_connection::KEEP_OPEN);
		ds = rom.dataspace();
		Dataspace_client client(ds);
		ptr = env()->rm_session()->attach(ds);
		printf("File size is %zx at %p\n", client.size(), ptr);
	}
	catch (...) {
		PDBG("Rom error");
		return 1;
	}

	Rm_connection rm(0, Dataspace_client(ds).size());
	rm.fault_handler(Pager::pager()->signal_receiver()->manage(&context));
	Pager::pager()->dataspace(ds);
	Pager::pager()->rm(&rm);

	/* start pager thread */
	Pager::pager()->start();

	uint8_t *ptr_nest = env()->rm_session()->attach(rm.dataspace());

	Rm_connection rm2(0, Dataspace_client(ds).size());
	rm2.fault_handler(Pager::pager2()->signal_receiver()->manage(&context2));
	Pager::pager2()->dataspace(rm.dataspace());
	Pager::pager2()->rm(&rm2);

	Pager::pager2()->start();

	uint8_t *ptr_nest2 = env()->rm_session()->attach(rm2.dataspace());

	dump(ptr, 0x1000);
	dump(ptr_nest, 0x2000);
	dump(ptr_nest2, 0x3000);

	dump(ptr, 0x10000);
	dump(ptr, 0x20000);

	dump(ptr, 0x1000);
	dump(ptr_nest, 0x2000);
	dump(ptr_nest2, 0x3000);

	try {

		Rom_connection rom("/notavail.txt");
		PERR("found file where no file should be!");
	}
	catch (...) {
		PDBG("Expected ROM error occured");
		return 1;
	}

	return 0;
}
