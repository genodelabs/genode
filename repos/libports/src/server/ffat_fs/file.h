/*
 * \brief  FFAT file-system file node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

#ifndef _FILE_H_
#define _FILE_H_

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

/* local includes */
#include <node.h>


namespace File_system {

	class File : public Node
	{
		private:

			Ffat::FIL _ffat_fil;

		public:

			File(const char *name) : Node(name) { }

			void ffat_fil(Ffat::FIL ffat_fil) { _ffat_fil = ffat_fil; }
			Ffat::FIL *ffat_fil() { return &_ffat_fil; }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
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

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
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

	};
}

#endif /* _FILE_H_ */
