/*
 * \brief  Utility for buffering XML nodes
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _OS__BUFFERED_XML_H_
#define _OS__BUFFERED_XML_H_

/* Genode includes */
#include <util/xml_node.h>

namespace Genode { class Buffered_xml; }


class Genode::Buffered_xml
{
	private:

		Allocator         &_alloc;
		char const * const _ptr;   /* pointer to dynamically allocated buffer */
		Xml_node     const _xml;   /* referring to buffer of '_ptr' */

		/**
		 * \throw Allocator::Out_of_memory
		 */
		static char const *_init_ptr(Allocator &alloc, Xml_node node)
		{
			char *ptr = (char *)alloc.alloc(node.size());
			Genode::memcpy(ptr, node.addr(), node.size());
			return ptr;
		}

		/*
		 * Noncopyable
		 */
		Buffered_xml(Buffered_xml const &);
		Buffered_xml &operator = (Buffered_xml const &);

	public:

		/**
		 * Constructor
		 *
		 * \throw Allocator::Out_of_memory
		 */
		Buffered_xml(Allocator &alloc, Xml_node node)
		:
			_alloc(alloc), _ptr(_init_ptr(alloc, node)), _xml(_ptr, node.size())
		{ }

		~Buffered_xml() { _alloc.free(const_cast<char *>(_ptr), _xml.size()); }

		Xml_node xml() const { return _xml; }
};

#endif /* _OS__BUFFERED_XML_H_ */
