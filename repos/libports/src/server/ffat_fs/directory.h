/*
 * \brief  FFAT file-system directory node
 * \author Christian Prochaska
 * \date   2012-07-04
 */

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

/* ffat includes */
namespace Ffat { extern "C" {
#include <ffat/ff.h>
} }

/* local includes */
#include <node.h>


namespace File_system {

	using namespace Genode;

	class Directory : public Node
	{
		private:

			Ffat::DIR _ffat_dir;
			int64_t   _prev_index;

		public:

			Directory(const char *name)
			: Node(name),
			  _prev_index(-1) { }

			void ffat_dir(Ffat::DIR ffat_dir) { _ffat_dir = ffat_dir; }
			Ffat::DIR *ffat_dir() { return &_ffat_dir; }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				if (len < sizeof(Directory_entry)) {
					error("read buffer too small for directory entry");
					return 0;
				}

				if (seek_offset % sizeof(Directory_entry)) {
					error("seek offset not alighed to sizeof(Directory_entry)");
					return 0;
				}

				Directory_entry *e = (Directory_entry *)(dst);

				using namespace Ffat;

				FILINFO ffat_file_info;
				ffat_file_info.lfname = e->name;
				ffat_file_info.lfsize = sizeof(e->name);

				int64_t index = seek_offset / sizeof(Directory_entry);

				if (index != (_prev_index + 1)) {
					/* rewind and iterate from the beginning */
					f_readdir(&_ffat_dir, 0);
					for (int i = 0; i < index; i++)
						f_readdir(&_ffat_dir, &ffat_file_info);
				}

				_prev_index = index;

				FRESULT res = f_readdir(&_ffat_dir, &ffat_file_info);
				switch(res) {
					case FR_OK:
						break;
					case FR_INVALID_OBJECT:
						error("f_readdir() failed with error code FR_INVALID_OBJECT");
						return 0;
					case FR_DISK_ERR:
						error("f_readdir() failed with error code FR_DISK_ERR");
						return 0;
					case FR_INT_ERR:
						error("f_readdir() failed with error code FR_INT_ERR");
						return 0;
					case FR_NOT_READY:
						error("f_readdir() failed with error code FR_NOT_READY");
						return 0;
					default:
						/* not supposed to occur according to the libffat documentation */
						error("f_readdir() returned an unexpected error code");
						return 0;
				}

				if (ffat_file_info.fname[0] == 0) { /* no (more) entries */
					return 0;
				}

				if (e->name[0] == 0) /* use short file name */
					strncpy(e->name, ffat_file_info.fname, sizeof(e->name));

				if ((ffat_file_info.fattrib & AM_DIR) == AM_DIR)
					e->type = Directory_entry::TYPE_DIRECTORY;
				else
					e->type = Directory_entry::TYPE_FILE;

				return sizeof(Directory_entry);
			}

			size_t write(char const *src, size_t len, seek_off_t)
			{
				/* writing to directory nodes is not supported */
				return 0;
			}

	};
}

#endif /* _DIRECTORY_H_ */
