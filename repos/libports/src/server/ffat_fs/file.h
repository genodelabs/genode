/*
 * \brief  FFAT file-system file node
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

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

/* local includes */
#include <node.h>


namespace Ffat_fs {
	class File;
}

class Ffat_fs::File : public Node
{
	private:

		Ffat::FIL _ffat_fil;

	public:

		File(const char *name) : Node(name) { }

		~File()
		{
			using namespace Ffat;

			FRESULT res = f_close(&_ffat_fil);

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
					/* not supposed to occur according to the libffat documentation */
					error("f_close() returned an unexpected error code");
					return;
			}
		}

		void ffat_fil(Ffat::FIL ffat_fil) { _ffat_fil = ffat_fil; }
		Ffat::FIL *ffat_fil() { return &_ffat_fil; }

		size_t read(char *dst, size_t len, seek_off_t seek_offset) override
		{
			using namespace Ffat;

			UINT result;

			if (seek_offset == (seek_off_t)(~0))
				seek_offset = _ffat_fil.fsize;

			FRESULT res = f_lseek(&_ffat_fil, seek_offset);

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
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_lseek() returned an unexpected error code");
					return 0;
			}

			res = f_read(&_ffat_fil, dst, len, &result);

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
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_read() returned an unexpected error code");
					return 0;
			}
		}

		size_t write(char const *src, size_t len, seek_off_t seek_offset) override
		{
			using namespace Ffat;

			UINT result;

			if (seek_offset == (seek_off_t)(~0))
				seek_offset = _ffat_fil.fsize;

			FRESULT res = f_lseek(&_ffat_fil, seek_offset);

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
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_lseek() returned an unexpected error code");
					return 0;
			}

			res = f_write(&_ffat_fil, src, len, &result);

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
					/* not supposed to occur according to the libffat documentation */
					Genode::error("f_write() returned an unexpected error code");
					return 0;
			}
		}

		void truncate(file_size_t size) override
		{
			using namespace Ffat;

			/*
 	 	 	 * This macro is defined in later versions of the FatFs lib, but not in the
 	 	 	 * one currently used for Genode.
 	 	 	 */
			#define f_tell(fp) ((fp)->fptr)

			/* 'f_truncate()' truncates to the current seek pointer */

			FRESULT res = f_lseek(&_ffat_fil, size);

			switch(res) {
				case FR_OK:
					/* according to the FatFs documentation this can happen */
					if (f_tell(&_ffat_fil) != size) {
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
					/* not supposed to occur according to the libffat documentation */
					error("f_lseek() returned an unexpected error code");
					return;
			}

			res = f_truncate(&_ffat_fil);

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
					/* not supposed to occur according to the libffat documentation */
					error("f_truncate() returned an unexpected error code");
					return;
			}
		}

};

#endif /* _FILE_H_ */
