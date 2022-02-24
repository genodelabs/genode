/*
 * \brief  Connect rump kernel to Genode's block interface
 * \author Sebastian Sumpf
 * \date   2013-12-16
 */

/*
 * Copyright (C) 2013-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include "sched.h"
#include <base/allocator_avl.h>
#include <base/registry.h>
#include <block_session/connection.h>
#include <rump/env.h>
#include <rump_fs/fs.h>

static const bool verbose = false;

enum  { GENODE_FD = 64 };


class Io_signal_blockade : public Genode::Io_signal_handler<Io_signal_blockade>
{
	private:

		Genode::Entrypoint &_ep;

		Genode::Thread const *_ep_thread_ptr;

		bool _signal_handler_called { false };

		typedef Genode::Registered_no_delete<Genode::Blockade> Registered_blockade;
		Genode::Registry<Registered_blockade> _blockades;

		void _handle_io_signal()
		{
			/* unblock the ep thread */
			_signal_handler_called = true;

			/* unblock non-ep threads */
			_blockades.for_each([] (Genode::Blockade &blockade) {
				blockade.wakeup();
			});
		}

	public:

		Io_signal_blockade(Entrypoint &ep)
		: Io_signal_handler(ep, *this,
		                    &Io_signal_blockade::_handle_io_signal),
		  _ep(ep)
		{
			_ep_thread_ptr = Genode::Thread::myself();
		}

		void block_for_io()
		{
			if (Genode::Thread::myself() == _ep_thread_ptr) {

				while (!_signal_handler_called)
					_ep.wait_and_dispatch_one_io_signal();

				_signal_handler_called = false;

			} else {

				Registered_blockade _blockade { _blockades };
				_blockade.block();

			}
		}
};


/**
 * Block session connection
 */
class Backend
{
	private:

		struct Job : Block::Connection<Job>::Job
		{
			void * const ptr;

			bool success = false;

			Job(Block::Connection<Job> &conn, void *ptr, Block::Operation operation)
			:
				Block::Connection<Job>::Job(conn, operation),
				ptr(ptr)
			{ }
		};

		Genode::Allocator_avl  _alloc   { &Rump::env().heap() };
		Genode::Entrypoint    &_ep      {  Rump::env().env().ep() };
		Block::Connection<Job> _session {  Rump::env().env(), &_alloc };
		Block::Session::Info   _info    { _session.info() };
		Genode::Mutex          _session_mutex;
		Io_signal_blockade     _io_signal_blockade { _ep };

		int _blocked_for_synchronous_io = 0;

		void _update_jobs()
		{
			Genode::Mutex::Guard guard(_session_mutex);

			struct Update_jobs_policy
			{
				void produce_write_content(Job &job, Block::seek_off_t offset,
				                           char *dst, size_t length)
				{
					Genode::memcpy(dst, job.ptr, length);
				}

				void consume_read_result(Job &job, Block::seek_off_t offset,
				                         char const *src, size_t length)
				{
					Genode::memcpy(job.ptr, src, length);
				}

				void completed(Job &job, bool success)
				{
					job.success = success;
				}

			} policy;

			_session.update_jobs(policy);
		}

		/*
		 * This function can be called by multiple threads
		 * (ep and 'pdaemon' have been observed so far in practice).
		 * A non-ep thread cannot dispatch signals and needs to block
		 * until the ep has processed the signal. Therefore it is
		 * important that the ep has the chance to process the signal
		 * even in the case that it calls this function while a non-ep
		 * thread is already blocking here. For this reason the
		 * '_session_mutex' cannot be held over the scope of the whole
		 * function.
		 */
		bool _synchronous_io(void *ptr, Block::Operation const &operation)
		{
			Genode::Constructible<Job> job { };

			{
				Genode::Mutex::Guard guard(_session_mutex);
				job.construct(_session, ptr, operation);
				_blocked_for_synchronous_io++;
			}

			_update_jobs();

			while (!job->completed()) {
				_io_signal_blockade.block_for_io();
				_update_jobs();
			}

			bool const success = job->success;

			{
				Genode::Mutex::Guard guard(_session_mutex);
				job.destruct();
				_blocked_for_synchronous_io--;
			}

			return success;
		}

	public:

		Backend()
		{
			_session.sigh(_io_signal_blockade);
		}

		uint64_t block_count() const { return _info.block_count; }
		size_t   block_size()  const { return _info.block_size; }
		bool     writeable()   const { return _info.writeable; }

		void sync()
		{
			Block::Operation const operation { .type = Block::Operation::Type::SYNC };

			(void)_synchronous_io(nullptr, operation);
		}

		bool submit(int op, int64_t offset, size_t length, void *data)
		{
			using namespace Block;

			Block::Operation const operation {
				.type         = (op & RUMPUSER_BIO_WRITE)
				              ? Block::Operation::Type::WRITE
				              : Block::Operation::Type::READ,
				.block_number = offset / _info.block_size,
				.count        = length / _info.block_size };

			bool const success = _synchronous_io(data, operation);

			/* sync request */
			if (op & RUMPUSER_BIO_SYNC)
				sync();

			return success;
		}

		bool blocked_for_io() const { return _blocked_for_synchronous_io > 0; }
};


static Backend &backend()
{
	static Backend _b;
	return _b;
}


int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
	if (Genode::strcmp(GENODE_BLOCK_SESSION, name))
		return ENXIO;

	if (type)
		*type = RUMPUSER_FT_BLK;

	if (size)
		*size = backend().block_count() * backend().block_size();

	return 0;
}


int rumpuser_open(const char *name, int mode, int *fdp)
{
	if (!(mode & RUMPUSER_OPEN_BIO || Genode::strcmp(GENODE_BLOCK_SESSION, name)))
		return ENXIO;

	/* check for writeable */
	if ((mode & RUMPUSER_OPEN_ACCMODE) && !backend().writeable())
		return EROFS;

	*fdp = GENODE_FD;
	return 0;
}


void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off, 
                  rump_biodone_fn biodone, void *donearg)
{
	int nlocks;
	rumpkern_unsched(&nlocks, 0);

	/* data request */
	if (verbose)
		Genode::log("fd: ",   fd,   " "
		            "op: ",   op,   " "
		            "len: ",  dlen, " "
		            "off: ",  Genode::Hex((unsigned long)off), " "
		            "bio ",   donearg, " "
		            "sync: ", !!(op & RUMPUSER_BIO_SYNC));

	bool succeeded = backend().submit(op, off, dlen, data);

	rumpkern_sched(nlocks, 0);

	if (biodone)
		biodone(donearg, dlen, succeeded ? 0 : EIO);
}


void rump_io_backend_sync()
{
	backend().sync();
}


bool rump_io_backend_blocked_for_io()
{
	return backend().blocked_for_io();
}


/* constructors in rump_fs.lib.so */
extern "C" void rumpcompctor_RUMP_COMPONENT_KERN_SYSCALL(void);
extern "C" void rumpcompctor_RUMP_COMPONENT_SYSCALL(void);
extern "C" void rumpcompctor_RUMP__FACTION_VFS(void);
extern "C" void rumpcompctor_RUMP__FACTION_DEV(void);
extern "C" void rumpns_modctor_cd9660(void);
extern "C" void rumpns_modctor_dk_subr(void);
extern "C" void rumpns_modctor_ext2fs(void);
extern "C" void rumpns_modctor_ffs(void);
extern "C" void rumpns_modctor_msdos(void);
extern "C" void rumpns_modctor_wapbl(void);


void rump_io_backend_init()
{
	/* call init/constructor functions of rump_fs.lib.so (order is important!) */
	rumpcompctor_RUMP_COMPONENT_KERN_SYSCALL();
	rumpns_modctor_wapbl();
	rumpcompctor_RUMP_COMPONENT_SYSCALL();
	rumpcompctor_RUMP__FACTION_VFS();
	rumpcompctor_RUMP__FACTION_DEV();
	rumpns_modctor_msdos();
	rumpns_modctor_ffs();
	rumpns_modctor_ext2fs();
	rumpns_modctor_dk_subr();
	rumpns_modctor_cd9660();

	/* create back end */
	backend();
}


void rumpuser_dprintf(const char *format, ...)
{
	va_list list;
	va_start(list, format);

	char buf[128] { };
	Genode::String_console(buf, sizeof(buf)).vprintf(format, list);
	Genode::log(Genode::Cstring(buf));

	va_end(list);
}
