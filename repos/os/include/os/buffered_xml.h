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
#include <util/retry.h>
#include <util/xml_node.h>
#include <util/xml_generator.h>
#include <base/allocator.h>

namespace Genode { class Buffered_xml; }


class Genode::Buffered_xml
{
	private:

		Allocator &_alloc;

		struct Allocation { char *ptr; size_t size; } const _allocation;

		Xml_node const _xml { _allocation.ptr, _allocation.size };

		/**
		 * \throw Allocator::Out_of_memory
		 */
		Allocation _copy_xml_node(Xml_node node)
		{
			Allocation allocation { };

			node.with_raw_node([&] (char const *start, size_t length) {
				allocation = Allocation { (char *)_alloc.alloc(length), length };
				Genode::memcpy(allocation.ptr, start, length);
			});

			return allocation;
		}

		/**
		 * Generate XML into allocated buffer
		 *
		 * \throw Allocation::Out_of_memory
		 */
		template <typename FN>
		Allocation _generate(char const *node_name, FN const &fn, size_t size)
		{
			Allocation allocation { };

			retry<Xml_generator::Buffer_exceeded>(

				[&] () {
					allocation = Allocation { (char *)_alloc.alloc(size), size };
					Xml_generator xml(allocation.ptr, size, node_name,
					                  [&] () { fn(xml); }); },

				[&] () {
					_alloc.free(allocation.ptr, allocation.size);
					allocation = { };
					size = size*2;
				}
			);
			return allocation;
		}

		/*
		 * Noncopyable
		 */
		Buffered_xml(Buffered_xml const &);
		Buffered_xml &operator = (Buffered_xml const &);

	public:

		/**
		 * Constructor for buffering a copy of the specified XML node
		 *
		 * \throw Allocator::Out_of_memory
		 */
		Buffered_xml(Allocator &alloc, Xml_node node)
		:
			_alloc(alloc), _allocation(_copy_xml_node(node))
		{ }

		struct Min_size { size_t value; };

		/**
		 * Constructor for buffering generated XML
		 *
		 * \param name  name of top-level XML node
		 * \param fn    functor that takes an 'Xml_generator &' as argument
		 * \param size  initial allocation size
		 *
		 * \throw Allocator::Out_of_memory
		 */
		template <typename FN>
		Buffered_xml(Allocator &alloc, char const *name, FN const &fn, Min_size size)
		:
			_alloc(alloc), _allocation(_generate(name, fn, size.value))
		{ }

		template <typename FN>
		Buffered_xml(Allocator &alloc, char const *name, FN const &fn)
		:
			Buffered_xml(alloc, name, fn, Min_size{4000})
		{ }

		~Buffered_xml() { _alloc.free(_allocation.ptr, _allocation.size); }

		/*
		 * \deprecated  Use 'with_xml_node' instead
		 */
		Xml_node xml() const { return _xml; }

		/**
		 * Call functor 'fn' with 'Xml_node const &' as argument
		 */
		template <typename FN>
		void with_xml_node(FN const &fn) const { fn(_xml); }
};

#endif /* _OS__BUFFERED_XML_H_ */
