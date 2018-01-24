/*
 * \brief  Utility for handling strings as AVL-node keys
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__AVL_STRING_H_
#define _INCLUDE__UTIL__AVL_STRING_H_

#include <util/avl_tree.h>
#include <util/string.h>

namespace Genode {
	
	class Avl_string_base;
	template <int> class Avl_string;
}


class Genode::Avl_string_base : public Avl_node<Avl_string_base>
{
	private:

		struct { const char *_str; };

	protected:

		/**
		 * Constructor
		 *
		 * \noapi
		 */
		Avl_string_base(const char *str) : _str(str) { }

	public:

		const char *name() const { return _str; }


		/************************
		 ** Avl node interface **
		 ************************/

		bool higher(Avl_string_base *c) { return (strcmp(c->_str, _str) > 0); }

		/**
		 * Find by name
		 */
		Avl_string_base *find_by_name(const char *name)
		{
			if (strcmp(name, _str) == 0) return this;

			Avl_string_base *c = Avl_node<Avl_string_base>::child(strcmp(name, _str) > 0);
			return c ? c->find_by_name(name) : 0;
		}
};


/*
 * The template pumps up the Avl_string_base object and provides the buffer for
 * the actual string.
 */
template <int STR_LEN>
class Genode::Avl_string : public Avl_string_base
{
	private:

		char _str_buf[STR_LEN];

	public:

		Avl_string(const char *str) : Avl_string_base(_str_buf)
		{
			strncpy(_str_buf, str, sizeof(_str_buf));
			_str_buf[STR_LEN - 1] = 0;
		}
};

#endif /* _INCLUDE__UTIL__AVL_STRING_H_ */
