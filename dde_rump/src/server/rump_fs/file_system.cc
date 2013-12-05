/*
 * \brief  Rump initialization
 * \author Sebastian Sumpf
 * \date   2014-01-17
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "file_system.h"

#include <base/thread.h>
#include <os/config.h>
#include <rump_fs/fs.h>
#include <timer_session/connection.h>
#include <util/string.h>
#include <util/hard_context.h>

/**
 * We define our own fs arg structure to fit all sizes, we assume that 'fspec'
 * is the only valid argument and all other fields are unused.
 */
struct fs_args
{
	char *fspec;
	char  pad[150];

	fs_args() { Genode::memset(pad, 0, sizeof(pad)); }
};

namespace File_system {
	class Sync;
};

static char const *fs_types[] = { RUMP_MOUNT_CD9660, RUMP_MOUNT_EXT2FS,
                                  RUMP_MOUNT_FFS, RUMP_MOUNT_MSDOS,
                                  RUMP_MOUNT_NFS, RUMP_MOUNT_NTFS,
                                  RUMP_MOUNT_SMBFS, RUMP_MOUNT_UDF, 0 };

static char _fs_type[10];
static bool _supports_symlinks;

static bool _check_type(char const *type)
{
	for (int i = 0; fs_types[i]; i++)
		if (!Genode::strcmp(type, fs_types[i]))
			return true;
	return false;
}


static void _print_types()
{
	PERR("fs types:");
	for (int i = 0; fs_types[i]; i++)
		PERR("\t%s", fs_types[i]);
}


static bool check_symlinks()
{
	if (!Genode::strcmp(_fs_type, RUMP_MOUNT_EXT2FS))
		return true;

	if (!Genode::strcmp(_fs_type, RUMP_MOUNT_FFS))
		return true;

	return false;
}


static bool check_read_only()
{
	if (!Genode::strcmp(_fs_type, RUMP_MOUNT_CD9660))
		return true;

	return false;
}


class File_system::Sync : public Genode::Thread<1024 * sizeof(Genode::addr_t)>
{
	private:

		Timer::Connection               _timer;
		Genode::Signal_rpc_member<Sync> _sync_dispatcher;

		void _process_sync(unsigned)
		{
			/* sync through front-end */
			rump_sys_sync();
			/* sync Genode back-end */
			rump_io_backend_sync();
		}

	protected:

		void entry()
		{
			while (1) {
				_timer.msleep(1000);
				/* send sync request, this goes to server entry point thread  */
				Genode::Signal_transmitter(_sync_dispatcher).submit();
			}
		}

	public:

		Sync(Server::Entrypoint &ep)
		:
			Thread("rump_fs_sync"),
			_sync_dispatcher(ep, *this, &Sync::_process_sync)
		{
				start(); 
		}
};


void File_system::init(Server::Entrypoint &ep)
{
	if (!Genode::config()->xml_node().attribute("fs").value(_fs_type, 10) ||
	    !_check_type(_fs_type)) {
		PERR("Invalid or no file system given (use \'<config fs=\"<fs type>\"/>)");
		_print_types();
		throw Genode::Exception();
	}
	PINF("Using %s as file system", _fs_type);

	/* start rump kernel */
	rump_init();

	/* register block device */ 
	rump_pub_etfs_register(GENODE_DEVICE, GENODE_BLOCK_SESSION, RUMP_ETFS_BLK);

	/* mount into extra-terrestrial-file system */
	struct fs_args args;
	int            opts = check_read_only() ? RUMP_MNT_RDONLY : 0;

	args.fspec =  (char *)GENODE_DEVICE;
	if (rump_sys_mount(_fs_type, "/", opts, &args, sizeof(args)) == -1) {
		PERR("Mounting '%s' file system failed (errno %u)", _fs_type, errno);
		throw Genode::Exception();
	}

	/* check support for symlinks */
	_supports_symlinks = check_symlinks();

	new (Genode::env()->heap()) Sync(ep);
}


bool File_system::supports_symlinks() { return _supports_symlinks; }


/*
 * On some platforms we end-up pulling in string.h prototypes
 */
extern "C" void *memcpy(void *d, void *s, size_t n)
{
	return Genode::memcpy(d, s, n);
}


extern "C" void *memset(void *s, int c, size_t n)
{
	return Genode::memset(s, c, n);
}
