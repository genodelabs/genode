/*
 * \brief  Buffer class
 * \author Josef Soentgen
 * \date   2014-01-15
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

/* Genode includes */
#include <util/string.h>

namespace Util {

	typedef Genode::size_t size_t;


	/**
	 * Buffer merely wrapps a simple char array
	 */
	template <size_t CAPACITY>
	class Buffer
	{
		public:

			class Out_of_range { };

		private:

			char   _buf[CAPACITY];
			size_t _length;

		public:

			static size_t capacity() { return CAPACITY; }

			Buffer() : _length(0) { }

			Buffer(char const *s)
			:
				_length(Genode::min(Genode::strlen(s) + 1, CAPACITY))
			{
				Genode::strncpy(_buf, s, _length);
			}

			char const *data() const { return (_buf[_length -1 ] == '\0') ? _buf : ""; }

			char *data() { return _buf; }

			size_t length() const { return _length; }

			char & operator[](size_t i)
			{
				if (i >= CAPACITY)
					throw Out_of_range();

				return _buf[i];
			}

			void replace(char p, char c)
			{
				char *s = _buf;
				for (; *s; s++) {
					if (*s == p)
						*s = c;
				}
			}
	};

	/**
	 * This class walks along a label and returns the next element on request
	 */
	class Label_walker
	{
		private:

			Buffer<64> _buffer;
			char const *_label;

			char const *_next()
			{
				size_t i = 0;

				for (; *_label && (i < _buffer.capacity()); _label++, i++) {
					/* check seperator */
					if ((*_label == ' ') &&
					    (*(_label + 1) == '-') &&
					    (*(_label + 2) == '>') &&
					    (*(_label + 3) == ' '))
						break;
					_buffer[i] = *_label;
				}

				_buffer[i] = '\0';

				/* sanatize the element */
				_buffer.replace('/', '_');

				/* omit seperator */
				if (*_label)
					_label += 4;

				return _label;
			}


		public:

			Label_walker(char const *label) : _label(label) { }

			/**
			 * Walk to the next element of the label
			 *
			 * \return pointer to the remaing part of the label
			 */
			char const *next() { return _next(); }

			/**
			 * Get current element of the label
			 *
			 * \return pointer to current element
			 */
			char const *element() { return _buffer.data(); }
	};
}

#endif /* _BUFFER_H_ */
