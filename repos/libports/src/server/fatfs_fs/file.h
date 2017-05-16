/*
 * \brief  FATFS file-system file node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_H_
#define _FILE_H_

/* fatfs includes */
namespace Fatfs { extern "C" {
#include <fatfs/ff.h>
} }

/* local includes */
#include <node.h>


namespace Fatfs_fs {
	class File;
}

class Fatfs_fs::File : public Node
{
	private:

		Fatfs::FIL _fatfs_fil;

	public:

		File(const char *name) : Node(name) { }

		~File()
		{
			using namespace Fatfs;

			FRESULT res = f_close(&_fatfs_fil);

			switch(res) {
				case FR_OK:
					return;
				case FR_INVALID_OBJECT:
					error("f_close() failed with error code FR_INVALID_OBJECT");
					return;
				case FR_DISK_ERR:
					error("f_close() failed with error code FR_DISK_ERR");
					return;
				case FR_INT_ERR:
					error("f_close() failed with error code FR_INT_ERR");
					return;
				case FR_NOT_READY:
					error("f_close() failed with error code FR_NOT_READY");
					return;
				default:
					/* not supposed to occur according to the fatfs documentation */
					error("f_close() returned an unexpected error code");
					return;
			}
		}

		void fatfs_fil(Fatfs::FIL fatfs_fil) { _fatfs_fil = fatfs_fil; }
		Fatfs::FIL *fatfs_fil() { return &_fatfs_fil; }

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			using namespace Fatfs;

			UINT result;

			if (seek_offset == (seek_off_t)(~0)) {
				FILINFO file_info;
				f_stat(name(), &file_info);

				seek_offset = file_info.fsize;
			}

			FRESULT res = f_lseek(&_fatfs_fil, seek_offset);

			switch(res) {
				case FR_OK:
					break;
				case FR_INVALID_OBJECT:
					Genode::error("f_lseek() failed with error code FR_INVALID_OBJECT");
					return 0;
				case FR_DISK_ERR:
					Genode::error("f_lseek() failed with error code FR_DISK_ERR");
					return 0;
				case FR_INT_ERR:
					Genode::error("f_lseek() failed with error code FR_INT_ERR");
					return 0;
				case FR_NOT_READY:
					Genode::error("f_lseek() failed with error code FR_NOT_READY");
					return 0;
				default:
					/* not supposed to occur according to the fatfs documentation */
					Genode::error("f_lseek() returned an unexpected error code");
					return 0;
			}

			res = f_read(&_fatfs_fil, dst, len, &result);

			switch(res) {
				case FR_OK:
					return result;
				case FR_DENIED:
					Genode::warning("f_read() failed with error code FR_DENIED");
					return 0;
				case FR_INVALID_OBJECT:
					Genode::error("f_read() failed with error code FR_INVALID_OBJECT");
					return 0;
				case FR_DISK_ERR:
					Genode::error("f_read() failed with error code FR_DISK_ERR");
					return 0;
				case FR_INT_ERR:
					Genode::error("f_read() failed with error code FR_INT_ERR");
					return 0;
				case FR_NOT_READY:
					Genode::error("f_read() failed with error code FR_NOT_READY");
					return 0;
				default:
					/* not supposed to occur according to the fatfs documentation */
					Genode::error("f_read() returned an unexpected error code");
					return 0;
			}
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			using namespace Fatfs;

			UINT result;

			if (seek_offset == (seek_off_t)(~0)) {
				FILINFO file_info;
				f_stat(name(), &file_info);

				seek_offset = file_info.fsize;
			}

			FRESULT res = f_lseek(&_fatfs_fil, seek_offset);

			switch(res) {
				case FR_OK:
					break;
				case FR_INVALID_OBJECT:
					Genode::error("f_lseek() failed with error code FR_INVALID_OBJECT");
					return 0;
				case FR_DISK_ERR:
					Genode::error("f_lseek() failed with error code FR_DISK_ERR");
					return 0;
				case FR_INT_ERR:
					Genode::error("f_lseek() failed with error code FR_INT_ERR");
					return 0;
				case FR_NOT_READY:
					Genode::error("f_lseek() failed with error code FR_NOT_READY");
					return 0;
				default:
					/* not supposed to occur according to the fatfs documentation */
					Genode::error("f_lseek() returned an unexpected error code");
					return 0;
			}

			res = f_write(&_fatfs_fil, src, len, &result);

			switch(res) {
				case FR_OK:
					return result;
				case FR_DENIED:
					Genode::error("f_write() failed with error code FR_DENIED");
					return 0;
				case FR_INVALID_OBJECT:
					Genode::error("f_write() failed with error code FR_INVALID_OBJECT");
					return 0;
				case FR_DISK_ERR:
					Genode::error("f_write() failed with error code FR_DISK_ERR");
					return 0;
				case FR_INT_ERR:
					Genode::error("f_write() failed with error code FR_INT_ERR");
					return 0;
				case FR_NOT_READY:
					Genode::error("f_write() failed with error code FR_NOT_READY");
					return 0;
				default:
					/* not supposed to occur according to the fatfs documentation */
					Genode::error("f_write() returned an unexpected error code");
					return 0;
			}
		}

		void truncate(file_size_t size) override
		{
			using namespace Fatfs;

			/*
 	 	 	 * This macro is defined in later versions of the FatFs lib, but not in the
 	 	 	 * one currently used for Genode.
 	 	 	 */
			#define f_tell(fp) ((fp)->fptr)

			/* 'f_truncate()' truncates to the current seek pointer */

			FRESULT res = f_lseek(&_fatfs_fil, size);

			switch(res) {
				case FR_OK:
					/* according to the FatFs documentation this can happen */
					if (f_tell(&_fatfs_fil) != size) {
						error("f_lseek() could not seek to offset ", size);
						return;
					}
					break;
				case FR_DISK_ERR:
					error("f_lseek() failed with error code FR_DISK_ERR");
					return;
				case FR_INT_ERR:
					error("f_lseek() failed with error code FR_INT_ERR");
					return;
				case FR_NOT_READY:
					error("f_lseek() failed with error code FR_NOT_READY");
					return;
				case FR_INVALID_OBJECT:
					error("f_lseek() failed with error code FR_INVALID_OBJECT");
					throw Invalid_handle();
				default:
					/* not supposed to occur according to the fatfs documentation */
					error("f_lseek() returned an unexpected error code");
					return;
			}

			res = f_truncate(&_fatfs_fil);

			switch(res) {
				case FR_OK:
					return;
				case FR_INVALID_OBJECT:
					error("f_truncate() failed with error code FR_INVALID_OBJECT");
					throw Invalid_handle();
				case FR_DISK_ERR:
					error("f_truncate() failed with error code FR_DISK_ERR");
					return;
				case FR_INT_ERR:
					error("f_truncate() failed with error code FR_INT_ERR");
					return;
				case FR_NOT_READY:
					error("f_truncate() failed with error code FR_NOT_READY");
					return;
				case FR_TIMEOUT:
					error("f_truncate() failed with error code FR_TIMEOUT");
					return;
				default:
					/* not supposed to occur according to the fatfs documentation */
					error("f_truncate() returned an unexpected error code");
					return;
			}
		}

};

#endif /* _FILE_H_ */
