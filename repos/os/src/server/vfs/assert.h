/*
 * \brief  VFS result checks
 * \author Emery Hemingway
 * \date   2015-08-19
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VFS__ASSERT_H_
#define _VFS__ASSERT_H_

/* Genode includes */
#include <vfs/directory_service.h>
#include <file_system_session/file_system_session.h>

namespace File_system {

	using namespace Vfs;

	static inline void assert_open(Directory_service::Open_result r)
	{
		typedef Directory_service::Open_result Result;

		switch (r) {
		case Result::OPEN_ERR_NAME_TOO_LONG: throw Invalid_name();
		case Result::OPEN_ERR_UNACCESSIBLE:  throw Lookup_failed();
		case Result::OPEN_ERR_NO_SPACE:      throw No_space();
		case Result::OPEN_ERR_NO_PERM:       throw Permission_denied();
		case Result::OPEN_ERR_EXISTS:        throw Node_already_exists();
		case Result::OPEN_ERR_OUT_OF_RAM:    throw Out_of_ram();
		case Result::OPEN_ERR_OUT_OF_CAPS:   throw Out_of_caps();
		case Result::OPEN_OK: break;
		}
	}

	static inline void assert_opendir(Directory_service::Opendir_result r)
	{
		typedef Directory_service::Opendir_result Result;

		switch (r) {
		case Result::OPENDIR_ERR_LOOKUP_FAILED:       throw Lookup_failed();
		case Result::OPENDIR_ERR_NAME_TOO_LONG:       throw Invalid_name();
		case Result::OPENDIR_ERR_NODE_ALREADY_EXISTS: throw Node_already_exists();
		case Result::OPENDIR_ERR_NO_SPACE:            throw No_space();
		case Result::OPENDIR_ERR_OUT_OF_RAM:          throw Out_of_ram();
		case Result::OPENDIR_ERR_OUT_OF_CAPS:         throw Out_of_caps();
		case Result::OPENDIR_ERR_PERMISSION_DENIED:   throw Permission_denied();
		case Result::OPENDIR_OK: break;
		}
	}

	static inline void assert_openlink(Directory_service::Openlink_result r)
	{
		typedef Directory_service::Openlink_result Result;

		switch (r) {
		case Result::OPENLINK_ERR_LOOKUP_FAILED:       throw Lookup_failed();
		case Result::OPENLINK_ERR_NAME_TOO_LONG:       throw Invalid_name();
		case Result::OPENLINK_ERR_NODE_ALREADY_EXISTS: throw Node_already_exists();
		case Result::OPENLINK_ERR_NO_SPACE:            throw No_space();
		case Result::OPENLINK_ERR_OUT_OF_RAM:          throw Out_of_ram();
		case Result::OPENLINK_ERR_OUT_OF_CAPS:         throw Out_of_caps();
		case Result::OPENLINK_ERR_PERMISSION_DENIED:   throw Permission_denied();
		case Result::OPENLINK_OK: break;
		}
	}

	static inline void assert_truncate(File_io_service::Ftruncate_result r)
	{
		typedef File_io_service::Ftruncate_result Result;

		switch (r) {
		case Result::FTRUNCATE_ERR_INTERRUPT: throw Invalid_handle();
		case Result::FTRUNCATE_ERR_NO_SPACE:  throw No_space();
		case Result::FTRUNCATE_ERR_NO_PERM:   throw Permission_denied();
		case Result::FTRUNCATE_OK: break;
		}
	}

	static inline void assert_unlink(Directory_service::Unlink_result r)
	{
		typedef Directory_service::Unlink_result Result;
		switch (r) {
		case Result::UNLINK_ERR_NO_ENTRY:  throw Lookup_failed();
		case Result::UNLINK_ERR_NO_PERM:   throw Permission_denied();
		case Result::UNLINK_ERR_NOT_EMPTY: throw Not_empty();
		case Result::UNLINK_OK: break;
		}
	}

	static inline void assert_stat(Directory_service::Stat_result r)
	{
		typedef Directory_service::Stat_result Result;
		switch (r) {
		case Result::STAT_ERR_NO_ENTRY: throw Lookup_failed();
		case Result::STAT_ERR_NO_PERM:  throw Permission_denied();
		case Result::STAT_OK: break;
		}
	}

	static inline void assert_rename(Directory_service::Rename_result r)
	{
		typedef Directory_service::Rename_result Result;
		switch (r) {
		case Result::RENAME_ERR_NO_ENTRY: throw Lookup_failed();
		case Result::RENAME_ERR_CROSS_FS: throw Permission_denied();
		case Result::RENAME_ERR_NO_PERM:  throw Permission_denied();
		case Result::RENAME_OK: break;
		}
	}
}

#endif /* _VFS__ASSERT_H_ */
