/*
 * \brief  Symlink file-system node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _SYMLINK_H_
#define _SYMLINK_H_

/* local includes */
#include <file.h>

namespace File_system {

	class Symlink : public File
	{
		public:

			Symlink(char const *name) : File(name) { }

			size_t read(char *dst, size_t len, seek_off_t seek_offset) {
				return 0; }

			size_t write(char const *src, size_t len, seek_off_t seek_offset) {
				return 0; }

			file_size_t length() const { return 0; }

			void truncate(file_size_t) { }
	};
}

#endif /* _SYMLINK_H_ */
