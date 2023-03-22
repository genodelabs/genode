/*
 * \brief   Low level disk I/O module using a Block session
 * \author  Christian Prochaska
 * \date    2011-05-30
 *
 * See doc/en/appnote.html in the FatFS source.
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <block_session/connection.h>
#include <base/allocator_avl.h>
#include <base/log.h>
#include <block/request.h>

/* Genode block backend */
#include <fatfs/block.h>


namespace Fatfs {

extern "C" {
/* fatfs includes */
#include <fatfs/diskio.h>
}

	using namespace Genode;

	struct Drive;
	struct Platform
	{
		Env &env;
		Allocator &alloc;
		Allocator_avl tx_alloc { &alloc };

		enum { MAX_DEV_NUM = 8 };

		/* XXX: could make a tree... */
		Drive* drives[MAX_DEV_NUM];

		Platform(Env &env, Allocator &alloc)
		: env(env), alloc(alloc)
		{
			for (int i = 0; i < MAX_DEV_NUM; ++i)
				drives[i] = nullptr;
		}
	};

	static Platform *_platform;

	void block_init(Env &env, Allocator &alloc)
	{
		static Platform platform { env, alloc };
		_platform = &platform;
	}


	struct Drive : private Block::Connection<>
	{
		Entrypoint &_ep;

		Info const info = Block::Connection<>::info();

		Io_signal_handler<Drive> _signal_handler { _ep, *this, &Drive::_io_dummy };

		struct Read_job : Job
		{
			char * const dst_ptr;

			Read_job(Drive &drive, char *dst_ptr,
			         Block::block_number_t sector, Block::block_count_t count)
			:
				Job(drive, Block::Operation { .type         = Block::Operation::Type::READ,
				                              .block_number = sector,
				                              .count        = count }),
				dst_ptr(dst_ptr)
			{ }
		};

		struct Write_job : Job
		{
			char const * const src_ptr;

			Write_job(Drive &drive, char const *src_ptr,
			          Block::block_number_t sector, Block::block_count_t count)
			:
				Job(drive, Block::Operation { .type         = Block::Operation::Type::WRITE,
				                              .block_number = sector,
				                              .count        = count }),
				src_ptr(src_ptr)
			{ }
		};

		void _io_dummy()
		{
			/* can be empty as only used to deblock wait_and_dispatch_one_io_signal() */
		}

		void _update_jobs()
		{
			struct Update_jobs_policy
			{
				void produce_write_content(Job &job, off_t offset,
				                           char *dst, size_t length)
				{
					Write_job &write_job = static_cast<Write_job &>(job);
					memcpy(dst, write_job.src_ptr + offset, length);
				}

				void consume_read_result(Job &job, off_t offset,
				                         char const *src, size_t length)
				{
					Read_job &read_job = static_cast<Read_job &>(job);
					memcpy(read_job.dst_ptr + offset, src, length);
				}

				void completed(Job &, bool success) { }

			} policy;

			update_jobs(policy);
		}

		using Block::Connection<>::tx;
		using Block::Connection<>::alloc_packet;

		void block_for_completion(Job const &job)
		{
			_update_jobs();
			while (!job.completed()) {
				_ep.wait_and_dispatch_one_io_signal();
				_update_jobs();
			}
		}

		void sync()
		{
			Block::Operation const operation { .type = Block::Operation::Type::SYNC };

			Job sync_job { *this, operation };

			block_for_completion(sync_job);
		}

		Drive(Platform &platform, char const *label)
		:
			Block::Connection<>(platform.env, &platform.tx_alloc, 128*1024, label),
			_ep(platform.env.ep())
		{
			sigh(_signal_handler);
		}
	};
}


using namespace Fatfs;


extern "C" Fatfs::DSTATUS disk_initialize (BYTE drv)
{
	if (drv >= Platform::MAX_DEV_NUM) {
		error("only ", (int)Platform::MAX_DEV_NUM," supported");
		return STA_NODISK;
	}

	if (_platform->drives[drv]) {
		destroy(_platform->alloc, _platform->drives[drv]);
		_platform->drives[drv] = nullptr;
	}

	try {
		String<2> label(drv);
		_platform->drives[drv] = new (_platform->alloc) Drive(*_platform, label.string());
	} catch(Service_denied) {
		error("could not open block connection for drive ", drv);
		return STA_NODISK;
	}

	Drive &drive = *_platform->drives[drv];

	/* check for write-capability */
	if (!drive.info.writeable)
		return STA_PROTECT;

	return 0;
}


extern "C" DSTATUS disk_status (BYTE drv)
{
	if (_platform->drives[drv]) {
		if (_platform->drives[drv]->info.writeable)
			return 0;
		return STA_PROTECT;
	}

	return STA_NOINIT;
}


extern "C" DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
	if (!_platform->drives[pdrv])
		return RES_NOTRDY;

	Drive &drive = *_platform->drives[pdrv];

	size_t const op_len = drive.info.block_size*count;

	Drive::Read_job read_job { drive, (char *)buff, sector, count };

	drive.block_for_completion(read_job);

	return RES_OK;
}


#if _READONLY == 0
extern "C" DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
	if (!_platform->drives[pdrv])
		return RES_NOTRDY;

	Drive &drive = *_platform->drives[pdrv];

	size_t const op_len = drive.info.block_size*count;

	Drive::Write_job write_job { drive, (char const *)buff, sector, count };

	drive.block_for_completion(write_job);

	return RES_OK;
}
#endif /* _READONLY */


extern "C" DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff)
{
	if (!_platform->drives[pdrv])
		return RES_NOTRDY;

	Drive &drive = *_platform->drives[pdrv];

	switch (cmd) {
	case CTRL_SYNC:
		drive.sync();
		return RES_OK;

	case GET_SECTOR_COUNT:
		*((DWORD*)buff) = drive.info.block_count;
		return RES_OK;

	case GET_SECTOR_SIZE:
		*((WORD*)buff) = drive.info.block_size;
		return RES_OK;

	case GET_BLOCK_SIZE:
		*((DWORD*)buff) = 1;
		return RES_OK;

	default:
		return RES_PARERR;
	}
}
