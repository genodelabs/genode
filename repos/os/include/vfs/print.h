/*
 * \brief  Definitions for printing VFS errors
 * \author Emery Hemingway
 * \date   2017-11-07
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VFS__PRINT_H_
#define _INCLUDE__VFS__PRINT_H_

#include <vfs/file_system.h>

namespace Genode {


static inline void print(Genode::Output &output, Vfs::Directory_service::Open_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::Directory_service::Open_result::x: output.out_string(#x); break

	switch (r) {
	CASE_PRINT(OPEN_OK);
	CASE_PRINT(OPEN_ERR_UNACCESSIBLE);
	CASE_PRINT(OPEN_ERR_NO_PERM);
	CASE_PRINT(OPEN_ERR_EXISTS);
	CASE_PRINT(OPEN_ERR_NAME_TOO_LONG);
	CASE_PRINT(OPEN_ERR_NO_SPACE);
	CASE_PRINT(OPEN_ERR_OUT_OF_RAM);
	CASE_PRINT(OPEN_ERR_OUT_OF_CAPS);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::Directory_service::Opendir_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::Directory_service::Opendir_result::x: output.out_string(#x); break

	switch (r) {
	CASE_PRINT(OPENDIR_OK);
	CASE_PRINT(OPENDIR_ERR_LOOKUP_FAILED);
	CASE_PRINT(OPENDIR_ERR_NAME_TOO_LONG);
	CASE_PRINT(OPENDIR_ERR_NODE_ALREADY_EXISTS);
	CASE_PRINT(OPENDIR_ERR_NO_SPACE);
	CASE_PRINT(OPENDIR_ERR_OUT_OF_RAM);
	CASE_PRINT(OPENDIR_ERR_OUT_OF_CAPS);
	CASE_PRINT(OPENDIR_ERR_PERMISSION_DENIED);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::Directory_service::Openlink_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::Directory_service::Openlink_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(OPENLINK_ERR_LOOKUP_FAILED);
		CASE_PRINT(OPENLINK_ERR_NAME_TOO_LONG);
		CASE_PRINT(OPENLINK_ERR_NODE_ALREADY_EXISTS);
		CASE_PRINT(OPENLINK_ERR_NO_SPACE);
		CASE_PRINT(OPENLINK_ERR_OUT_OF_RAM);
		CASE_PRINT(OPENLINK_ERR_OUT_OF_CAPS);
		CASE_PRINT(OPENLINK_ERR_PERMISSION_DENIED);
		CASE_PRINT(OPENLINK_OK);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::Directory_service::Stat_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::Directory_service::Stat_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(STAT_OK);
		CASE_PRINT(STAT_ERR_NO_ENTRY);
		CASE_PRINT(STAT_ERR_NO_PERM);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::Directory_service::Unlink_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Unlink_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(UNLINK_OK);
		CASE_PRINT(UNLINK_ERR_NO_ENTRY);
		CASE_PRINT(UNLINK_ERR_NO_PERM);
		CASE_PRINT(UNLINK_ERR_NOT_EMPTY);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::Directory_service::Rename_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Rename_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(RENAME_OK);
		CASE_PRINT(RENAME_ERR_NO_ENTRY);
		CASE_PRINT(RENAME_ERR_CROSS_FS);
		CASE_PRINT(RENAME_ERR_NO_PERM);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::File_io_service::Write_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Write_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(WRITE_OK);
		CASE_PRINT(WRITE_ERR_AGAIN);
		CASE_PRINT(WRITE_ERR_WOULD_BLOCK);
		CASE_PRINT(WRITE_ERR_INVALID);
		CASE_PRINT(WRITE_ERR_IO);
		CASE_PRINT(WRITE_ERR_INTERRUPT);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::File_io_service::Read_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Read_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(READ_OK);
		CASE_PRINT(READ_ERR_AGAIN);
		CASE_PRINT(READ_ERR_WOULD_BLOCK);
		CASE_PRINT(READ_ERR_INVALID);
		CASE_PRINT(READ_ERR_IO);
		CASE_PRINT(READ_ERR_INTERRUPT);
		CASE_PRINT(READ_QUEUED);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::File_io_service::Ftruncate_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Ftruncate_result::x: output.out_string(#x); break

	switch (r) {
		CASE_PRINT(FTRUNCATE_OK);
		CASE_PRINT(FTRUNCATE_ERR_NO_PERM);
		CASE_PRINT(FTRUNCATE_ERR_INTERRUPT);
		CASE_PRINT(FTRUNCATE_ERR_NO_SPACE);
	}

#undef CASE_PRINT
}


static inline void print(Genode::Output &output, Vfs::File_io_service::Sync_result const &r)
{
#define CASE_PRINT(x) \
	case Vfs::File_io_service::Sync_result::x: output.out_string(#x); break

	switch (r) {
	CASE_PRINT(SYNC_OK);
	CASE_PRINT(SYNC_QUEUED);
	}

#undef CASE_PRINT
}

}

#endif /* _INCLUDE__VFS__PRINT_H_ */
