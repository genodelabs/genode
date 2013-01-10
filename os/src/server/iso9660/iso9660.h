/*
 * \brief  ISO interface to session server
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-26
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <rom_session/rom_session.h>
#include <base/stdint.h>

namespace Iso {

	/*
	 * Exceptions
	 */
	class Io_error : public Genode::Exception { };
	class Non_data_disc : public Genode::Exception { };
	class File_not_found : public Genode::Exception { };

	/* enable/disable debugging output */
	const int verbose = 0;

	enum {
		PATH_LENGTH  = 128, /* max. length of a path */
		LEVEL_LENGTH = 32,  /* max. length of a level of a path */
		PAGE_SIZE    = 4096,
	};


	class File_info
	{
		private:

			Genode::uint32_t _blk_nr;
			Genode::size_t   _size;

		public:

			File_info(Genode::uint32_t blk_nr, Genode::size_t size)
			: _blk_nr(blk_nr), _size(size) {}

			Genode::uint32_t blk_nr() { return _blk_nr; }
			Genode::size_t   size()   { return _size;   }
			Genode::size_t   page_sized() { return (_size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); }
	};


	/**
	 * Retrieve file information
	 *
	 * \param path absolute path of the file (slash separated)
	 *
	 * \throw File_not_found
	 * \throw Io_error
	 * \throw Non_data_disc
	 *
	 * \return Pointer to File_info class
	 */
	File_info *file_info(char *path);

	/**
	 * Read data from ISO
	 *
	 * \param info File    Info of file to read the data from
	 * \param file_offset  Offset in file
	 * \param length       Number of bytes to read
	 * \param buf          Output buffer
	 *
	 * \throw Io_error
	 *
	 * \return Number of bytes read
	 */
	unsigned long read_file(File_info *info, Genode::off_t file_offset,
	                        Genode::uint32_t length, void *buf);
}
