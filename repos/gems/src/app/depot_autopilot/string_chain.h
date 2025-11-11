/*
 * \brief  Dynamically growing buffer for aggregating strings
 * \author Norman Feske
 * \author Martin Stein
 * \date   2025-11-12
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _STRING_CHAIN_H_
#define _STRING_CHAIN_H_

#include <util/fifo.h>

/* local includes */
#include <types.h>

namespace Depot_autopilot { struct String_chain; }


struct Depot_autopilot::String_chain
{
	struct Element : Fifo<Element>::Element
	{
		Allocator &_alloc;

		Byte_range_ptr const bytes;

		static Byte_range_ptr _copied(Allocator &alloc, Span const &span)
		{
			char *dst = (char *)alloc.alloc(span.num_bytes);
			memcpy(dst, span.start, span.num_bytes);
			return { dst, span.num_bytes };
		}

		Element(Allocator &alloc, Span const &span)
		: _alloc(alloc), bytes(_copied(alloc, span)) { }

		~Element() { _alloc.free(bytes.start, bytes.num_bytes); }

		void print(Output &out) const { out.out_string(bytes.start, bytes.num_bytes); }
	};

	Allocator     &_alloc;
	Fifo<Element> _elements { };

	String_chain(Allocator &alloc) : _alloc(alloc) { }

	~String_chain() { reset(); }

	void reset()
	{
		_elements.dequeue_all([&] (Element &element) {
			destroy(_alloc, &element); });
	}

	void append(Span const &span)
	{
		_elements.enqueue(*new (_alloc) Element(_alloc, span));
	}

	/**
	 * Call 'fn' with matching span and element-relative offset
	 */
	auto with_span_at(size_t offset, auto const &fn,
	                  auto const &missing_fn) const -> decltype(missing_fn())
	{
		char const *start_ptr = nullptr;
		size_t      num_bytes = 0;

		_elements.for_each([&] (Element const &e) {

			if (start_ptr)
				return;

			if (offset >= e.bytes.num_bytes) {
				offset -= e.bytes.num_bytes; /* keep searching */
				return;
			}

			/* found element, determine span within the element's bytes */
			start_ptr = e.bytes.start     + offset;
			num_bytes = e.bytes.num_bytes - offset;
		});

		if (start_ptr && num_bytes)
			return fn(Span { start_ptr, num_bytes }, offset);

		return missing_fn();
	}

	size_t num_bytes() const
	{
		size_t n = 0;
		_elements.for_each([&] (Element const &e) { n += e.bytes.num_bytes; });
		return n;
	}
};

#endif /* _STRING_CHAIN_H__ */
