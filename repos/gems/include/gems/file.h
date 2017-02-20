/*
 * \brief  Utility for loading a file
 * \author Norman Feske
 * \date   2014-08-14
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__GEMS__FILE_H_
#define _INCLUDE__GEMS__FILE_H_

/* Genode includes */
#include <base/allocator.h>

class File
{
	private:

		Genode::Allocator   &_alloc;
		Genode::size_t const _file_size;
		void                *_data;

	public:

		/**
		 * Exception type
		 */
		class Reading_failed { };

		/**
		 * Constructor
		 *
		 * \throw Reading_failed;
		 */
		File(char const *name, Genode::Allocator &alloc);

		~File();

		template <typename T> T *data() { return (T *)_data; }

		Genode::size_t size() const { return _file_size; }
};

#endif /* _INCLUDE__GEMS__FILE_H_ */

