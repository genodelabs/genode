/*
 * \brief  Rom-session server for ISO file systems
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <cap_session/connection.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <root/component.h>
#include <rm_session/connection.h>
#include <util/avl_string.h>
#include <util/misc_math.h>

/* local includes */
#include "iso9660.h"
#include "backing_store.h"

using namespace Genode;


/*****************
 ** ROM service **
 *****************/

namespace Iso {

	typedef Backing_store<Genode::off_t> Backing_store;
	typedef Avl_string<PATH_LENGTH>      File_base;

	/**
	 * File abstraction
	 */
	class File : public File_base,
	             public Signal_context,
	             public Backing_store::User
	{

		private:

			File_info                *_info;
			Rm_connection            *_rm;
			Signal_receiver          *_receiver;
			Backing_store            *_backing_store;

		public:

			File(char *path, Signal_receiver *receiver, Backing_store *backing_store)
			: File_base(path),
				_info(Iso::file_info(path)),
				_receiver(receiver),
				_backing_store(backing_store)
			{
				size_t rm_size = align_addr(_info->page_sized(),
				                            log2(_backing_store->block_size()));
				
				_rm = new(env()->heap()) Rm_connection(0, rm_size);
				_rm->fault_handler(receiver->manage(this));
			}
			
			~File()
			{
				_receiver->dissolve(this);
				_backing_store->flush(this);
				destroy(env()->heap(), _rm);
				destroy(env()->heap(), _info);
			}

			Rm_connection *rm() { return _rm; }

			void handle_fault()
			{
				Rm_session::State state = _rm->state();

				if (verbose)
					printf("rm session state is %s, pf_addr=0x%lx\n",
					       state.type == Rm_session::READ_FAULT  ? "READ_FAULT"  :
					       state.type == Rm_session::WRITE_FAULT ? "WRITE_FAULT" :
					       state.type == Rm_session::EXEC_FAULT  ? "EXEC_FAULT"  : "READY",
					       state.addr);

				if (state.type == Rm_session::READY)
					return;

				Backing_store::Block *block = _backing_store->alloc();

				/*
				 * Calculate backing-store-block-aligned file offset from
				 * page-fault address.
				 */
				Genode::off_t file_offset = state.addr;
				file_offset &= ~(_backing_store->block_size() - 1);

				/* re-initialize block content */
				Genode::memset(_backing_store->local_addr(block), 0,
				               _backing_store->block_size());

				/* read file content to block */
				unsigned long bytes = Iso::read_file(_info, file_offset,
				                                     _backing_store->block_size(),
				                                     _backing_store->local_addr(block));

				if (verbose)
					PDBG("[%ld] ATTACH: rm=%p, a=%08lx s=%lx",
					     _backing_store->index(block), _rm, file_offset, bytes);

				bool try_again;
				do {
					try_again = false;
					try {
						_rm->attach_at(_backing_store->dataspace(), file_offset,
						               _backing_store->block_size(),
						               _backing_store->offset(block)); }

					catch (Genode::Rm_session::Region_conflict) {
						PERR("Region conflict - this should not happen"); }

					catch (Genode::Rm_session::Out_of_metadata) {

						/* give up if the error occurred a second time */
						if (try_again)
							break;

						PINF("upgrade quota donation for RM session");
						Genode::env()->parent()->upgrade(_rm->cap(), "ram_quota=32K");
						try_again = true;
					}
				} while (try_again);

				/*
				 * Register ourself as user of the block and thereby enable
				 * future eviction.
				 */
				_backing_store->assign(block, this, file_offset);
			}

			/**************************
			 ** File cache interface **
			 **************************/

			/** 
			 * File cache that holds files in order to re-use
			 * them in different sessions that request already cached files
			 */
			static Avl_tree<Avl_string_base> *cache()
			{
				static Avl_tree<Avl_string_base> _avl;
				return &_avl;
			}

			static File *scan_cache(const char *path) {
				return static_cast<File *>(cache()->first() ?
				                           cache()->first()->find_by_name(path) :
				                           0);
			}


			/***********************************
			 ** Backing_store::User interface **
			 ***********************************/

			/**
			 * Called by backing store if block gets evicted
			 */
			void detach_block(Genode::off_t file_offset)
			{
				_rm->detach((void *)file_offset);
			}
	};


	class Rom_component : public Genode::Rpc_object<Rom_session>
	{
		private:

			File *_file;

		public:

			Rom_dataspace_capability dataspace() {
				return static_cap_cast<Rom_dataspace>(_file->rm()->dataspace()); }

			void sigh(Signal_context_capability) { }

			Rom_component(char *path, Signal_receiver *receiver,
			              Backing_store *backing_store)
			{
				if ((_file = File::scan_cache(path))) {
					PINF("cache hit for file %s", path);
					return;
				}

				_file = new(env()->heap()) File(path, receiver, backing_store);
				PINF("request for file %s", path);

				File::cache()->insert(_file);
			}
	};


	class Pager : public Thread<8192>
	{
		private:

			Signal_receiver _receiver;

		public:

			Signal_receiver *signal_receiver() { return &_receiver; }

			void entry()
			{
				while (true) {
					try {
						Signal signal = _receiver.wait_for_signal();

						for (int i = 0; i < signal.num(); i++)
							static_cast<File *>(signal.context())->handle_fault();
					} catch (...) {
						PDBG("unexpected error while waiting for signal");
					}
				}
			}

			static Pager* pager()
			{
				static Pager _pager;
				return &_pager;
			}
	};


	typedef Root_component<Rom_component> Root_component;

	class Root : public Root_component
	{
		private:

			char _path[PATH_LENGTH];

			Backing_store *_backing_store;

		protected:

			Rom_component *_create_session(const char *args)
			{
				size_t ram_quota =
					Arg_string::find_arg(args, "ram_quota").ulong_value(0);
				size_t session_size =  sizeof(Rom_component) + sizeof(File_info) + sizeof(Rm_connection);
				if (ram_quota < session_size)
					throw Root::Quota_exceeded();

				Arg_string::find_arg(args,
				                     "filename").string(_path,
				                                        sizeof(_path), "");

				if (verbose)
					PDBG("Request for file %s lrn %zu", _path, strlen(_path));

				try {
					return new (md_alloc())
						Rom_component(_path, Pager::pager()->signal_receiver(),
						              _backing_store);
				}
				catch (Io_error)       { throw Root::Unavailable(); }
				catch (Non_data_disc)  { throw Root::Unavailable(); }
				catch (File_not_found) { throw Root::Invalid_args(); }
			}

		public:

			Root(Rpc_entrypoint *ep, Allocator *md_alloc,
			     Backing_store *backing_store)
			:
				Root_component(ep, md_alloc), _backing_store(backing_store)
			{ }
	};
}


int main()
{
	/*
	 * XXX this could be a config parameter
	 */
	const Genode::size_t backing_store_block_size = 8*Iso::PAGE_SIZE;

	enum { RESERVED_RAM = 5*1024*1024 };
	const Genode::size_t use_ram = env()->ram_session()->avail() - RESERVED_RAM;
	static Iso::Backing_store backing_store(use_ram, backing_store_block_size);

	/* start pager thread */
	Iso::Pager::pager()->start();

	/* initialize ROM service */
	enum { STACK_SIZE = 4096 };
	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, STACK_SIZE, "iso9660_ep");

	static Iso::Root root(&ep, env()->heap(), &backing_store);
	env()->parent()->announce(ep.manage(&root));

	sleep_forever();
	return 0;
}


