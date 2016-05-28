/*
 * \brief  VFS result checks
 * \author Emery Hemingway
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VFS__ASSERT_H_
#define _VFS__ASSERT_H_

/* Genode includes */
#include <vfs/directory_service.h>
#include <file_system_session/file_system_session.h>

static bool const verbose = false;
#define PDBGV(...) if (verbose) PDBG(__VA_ARGS__)
#define PERRV(...) if (verbose) PERR(__VA_ARGS__)

#define PERR_THROW(e) PERRV("%s - %s", __func__, #e); throw e();

namespace File_system {

	using namespace Vfs;

	static inline void assert_mkdir(Directory_service::Mkdir_result r)
	{
		typedef Directory_service::Mkdir_result Result;

		switch (r) {
		case Result::MKDIR_ERR_NAME_TOO_LONG: PERR_THROW(Name_too_long);
		case Result::MKDIR_ERR_NO_ENTRY:      PERR_THROW(Lookup_failed);
		case Result::MKDIR_ERR_NO_SPACE:      PERR_THROW(No_space);
		case Result::MKDIR_ERR_NO_PERM:       PERR_THROW(Permission_denied);
		case Result::MKDIR_ERR_EXISTS:        PERR_THROW(Node_already_exists);
		case Result::MKDIR_OK: break;
		}
	}

	static inline void assert_open(Directory_service::Open_result r)
	{
		typedef Directory_service::Open_result Result;

		switch (r) {
		case Result::OPEN_ERR_NAME_TOO_LONG: PERR_THROW(Invalid_name);
		case Result::OPEN_ERR_UNACCESSIBLE:  PERR_THROW(Lookup_failed);
		case Result::OPEN_ERR_NO_SPACE:      PERR_THROW(No_space);
		case Result::OPEN_ERR_NO_PERM:       PERR_THROW(Permission_denied);
		case Result::OPEN_ERR_EXISTS:        PERR_THROW(Node_already_exists);
		case Result::OPEN_OK: break;
		}
	}

	static inline void assert_symlink(Directory_service::Symlink_result r)
	{
		typedef Directory_service::Symlink_result Result;

		switch (r) {
		case Result::SYMLINK_ERR_NAME_TOO_LONG: PERR_THROW(Invalid_name);
		case Result::SYMLINK_ERR_NO_ENTRY:      PERR_THROW(Lookup_failed);
		case Result::SYMLINK_ERR_NO_SPACE:      PERR_THROW(No_space);
		case Result::SYMLINK_ERR_NO_PERM:       PERR_THROW(Permission_denied);
		case Result::SYMLINK_ERR_EXISTS:        PERR_THROW(Node_already_exists);
		case Result::SYMLINK_OK: break;
		}
	}

	static inline void assert_readlink(Directory_service::Readlink_result r)
	{
		typedef Directory_service::Readlink_result Result;

		switch (r) {
		case Result::READLINK_ERR_NO_ENTRY: PERR_THROW(Lookup_failed);
		case Result::READLINK_ERR_NO_PERM:  PERR_THROW(Permission_denied);
		case Result::READLINK_OK: break;
		}
	}

	static inline void assert_truncate(File_io_service::Ftruncate_result r)
	{
		typedef File_io_service::Ftruncate_result Result;

		switch (r) {
		case Result::FTRUNCATE_ERR_INTERRUPT: PERR_THROW(Invalid_handle);
		case Result::FTRUNCATE_ERR_NO_SPACE:  PERR_THROW(No_space);
		case Result::FTRUNCATE_ERR_NO_PERM:   PERR_THROW(Permission_denied);
		case Result::FTRUNCATE_OK: break;
		}
	}

	static inline void assert_unlink(Directory_service::Unlink_result r)
	{
		typedef Directory_service::Unlink_result Result;
		switch (r) {
		case Result::UNLINK_ERR_NO_ENTRY:  PERR_THROW(Lookup_failed);
		case Result::UNLINK_ERR_NO_PERM:   PERR_THROW(Permission_denied);
		case Result::UNLINK_ERR_NOT_EMPTY: PERR_THROW(Not_empty);
		case Result::UNLINK_OK: break;
		}
	}

	static inline void assert_stat(Directory_service::Stat_result r)
	{
		typedef Directory_service::Stat_result Result;
		switch (r) {
		case Result::STAT_ERR_NO_ENTRY: PERR_THROW(Lookup_failed);
		case Result::STAT_ERR_NO_PERM:  PERR_THROW(Permission_denied);
		case Result::STAT_OK: break;
		}
	}

	static inline void assert_rename(Directory_service::Rename_result r)
	{
		typedef Directory_service::Rename_result Result;
		switch (r) {
		case Result::RENAME_ERR_NO_ENTRY: PERR_THROW(Lookup_failed);
		case Result::RENAME_ERR_CROSS_FS: PERR_THROW(Permission_denied);
		case Result::RENAME_ERR_NO_PERM:  PERR_THROW(Permission_denied);
		case Result::RENAME_OK: break;
		}
	}
}

#endif /* _VFS__ASSERT_H_ */
