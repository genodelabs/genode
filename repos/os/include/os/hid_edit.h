/*
 * \brief  HID editing utility
 * \author Norman Feske
 * \date   2026-03-31
 *
 * 'Hid_edit' allows for the manipulation of HID data. The original version is
 * defined at construction time by loading it into a dynamically allocted
 * buffer. Once constructed, a sequence of editing operations can be applied.
 * Internally, each operation transforms the data into a new version targeting
 * a second buffer, and swaps both buffer. The final result can be obtained
 * via 'with_result'.
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__HID_EDIT_H_
#define _INCLUDE__OS__HID_EDIT_H_

/* Genode includes */
#include <base/node.h>
#include <base/allocator.h>

namespace Genode { struct Hid_edit; }


class Genode::Hid_edit : Noncopyable
{
	public:

		using Query = String<64>;

	private:

		using Alloc_result = Allocator::Alloc_result;
		using Allocation   = Allocator::Allocation;

		using Value = String<128>; /* type for comparing attributes */

		Allocator &_alloc;

		size_t const _limit;

		Generator::Result _content = 0;

		Alloc_result _bytes[2] { Alloc_error::DENIED, Alloc_error::DENIED };

		bool _b0_to_b1 = true; /* direction of the next edit operation */

		void _swap() { _b0_to_b1 = !_b0_to_b1; }

		void _realloc_dst(Alloc_result &a)
		{
			size_t const curr_limit =
				a.convert<size_t>([&] (Allocation &a) { return a.num_bytes; },
				                  [&] (Alloc_error)   { return 0ul; });

			if (curr_limit < _limit)
				a = _alloc.try_alloc(_limit);
		}

		void _with_src_dst(auto const &fn)
		{
			unsigned const src_i = _b0_to_b1 ? 0 : 1,
			               dst_i = _b0_to_b1 ? 1 : 0;

			_realloc_dst(_bytes[dst_i]);
			_bytes[src_i].with_result([&] (Allocation &src) {
				_bytes[dst_i].with_result([&] (Allocation &dst) {
					fn(Const_byte_range_ptr((char *)src.ptr, src.num_bytes),
					   Byte_range_ptr((char *)dst.ptr, dst.num_bytes));
				}, [&] (Alloc_error) { });
			}, [&] (Alloc_error) { });
		}

		void _filter_rec(Node const &query, Node const &from,
		                 Generator &g, auto const &match_fn)
		{
			bool match = query.type() == from.type();
			query.for_each_attribute([&] (Node::Attribute const &a) {
				if (!from.has_attribute(a.name.string()))
					match = false;

				Value const expected { Cstring(a.value.start, a.value.num_bytes) };
				if (from.attribute_value(a.name.string(), Value()) != expected)
					match = false;
			});

			/* whenever query does not apply, copy content as is */
			if (!match) {
				g.node_attributes(from);
				(void)g.append_node_content(from, Generator::Max_depth { 20 });
				return;
			}

			bool const end_of_query = (query.num_sub_nodes() == 0);

			if (end_of_query) {
				match_fn(query, from, g);  /* perform edit */
				return;
			}

			/* traverse into query */
			query.for_each_sub_node([&] (Node const &query) {
				g.node_attributes(from);
				from.for_each_sub_node([&] (Node const &from) {
					g.node(from.type().string(), [&] {
						_filter_rec(query, from, g, match_fn); }); }); });
		}

		void _filter(Query hid_query, auto const &fn)
		{
			if (_content.failed()) /* skip edits after first error */
				return;

			hid_query = { hid_query, "\n-" };

			_with_src_dst([&] (Const_byte_range_ptr const &src, Byte_range_ptr const &dst) {

				Node query { hid_query };
				Node from  { src };

				/* top-level type of query must match */
				if (query.type() != from.type())
					return;

				_content = Generator::generate(dst, from.type(), [&] (Generator &g) {
					_filter_rec(query, from, g, fn); });
			});

			if (_content.ok())
				_swap();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param fn  functor called to populate the allocated buffer with
		 *            the initial content. The allocated buffer is passed
		 *            as a 'Byte_range_ptr const &'. The functor returns the
		 *            size of the content in bytes.
		 */
		Hid_edit(Allocator &alloc, size_t buffer_size, auto const &fn)
		:
			_alloc(alloc), _limit(buffer_size)
		{
			_bytes[0] = _alloc.try_alloc(_limit);
			_bytes[0].with_result(
				[&] (Allocation &a) {
					size_t const n = fn(Byte_range_ptr((char *)a.ptr, a.num_bytes));
					if (n && n <= a.num_bytes)
						_content = n;
					else
						_bytes[0] = Alloc_error::DENIED; /* release allocation */
				},
				[&] (Alloc_error) { });
		}

		/**
		 * Adjust attribute value(s)
		 *
		 * The 'query' is the HID path describing the node(s) to edit, followed
		 * by the name of the targeted attribute as quoted content. The default
		 * value specifies the type and default value of the attribute. The
		 * functor 'fn' is called with the old attribute value as argument and
		 * expects a new attribute value as return value. Should multiple nodes
		 * satisfy the 'query', each match is adjusted.
		 *
		 * Even though the 'query' uses HID syntax, it does not require an end
		 * marker.
		 *
		 * For an example, the following operation increments the 'count'
		 * attribute within the nested 'connect' node(s). Only connect node(s)
		 * within a 'child' node named 'fb' are considered. If the attribute
		 * is not yet present, the value '123' is assumed as default.
		 *
		 * ! adjust("option | + child fb | + connect | : count", 123,
		 * !   [&] (int v) { return v + 1; });
		 */
		void adjust(Query const &query, auto const default_value, auto const &fn)
		{
			_filter(query, [&] (Node const &query, Node const &from, Generator &g) {

				Node::Attribute::Name tag { Node::Quoted_content { query } };

				/* modify attribute in place */
				bool modified_attr = false;
				from.for_each_attribute([&] (Node::Attribute const &a) {
					if (a.name == tag) {
						auto orig = from.attribute_value(tag.string(), default_value);
						g.attribute(a.name.string(), fn(orig));
						modified_attr = true;
					} else {
						g.attribute(a.name.string(), a.value.start, a.value.num_bytes);
					}
				});

				/* append new attribute */
				if (!modified_attr) {
					auto orig = from.attribute_value(tag.string(), default_value);
					g.attribute(tag.string(), fn(orig));
				}

				(void)g.append_node_content(from, Generator::Max_depth { 20 });
			});
		}

		/**
		 * Replace node(s) addressed by 'query'
		 *
		 * The query describes the path to the node to modify, followed by
		 * a selector given as quoted content. The selector is a node type
		 * followed by attribute definitions. Each node at the queried path
		 * that matches the type and attributes of the selector is replaced by
		 * the content generated by 'fn'. The functor 'fn' is called with the
		 * to-be-replaced 'Node const &' and a 'Generator &' as arguments.
		 *
		 * The following example replaces the 'fs rw' node by an 'fs ro' node.
		 *
		 * ! replace("option | + child fb | + connect | : fs rw",
		 * !   [&] (Node const &, Generator &g) {
		 * !      g.node("fs", [&] { g.attribute("name", "ro"); });
		 * !   });
		 */
		void replace(Query const &query, auto const &fn)
		{
			_filter(query, [&] (Node const &query, Node const &from, Generator &g) {

				Query const selector_hid { Node::Quoted_content { query }, "\n-" };

				Node const selector { selector_hid };

				auto selected = [&] (Node const &n)
				{
					if (n.type() != selector.type())
						return false;

					bool result = true;
					selector.for_each_attribute([&] (Node::Attribute const &a) {
						Value const value { Cstring(a.value.start, a.value.num_bytes) };
						if (n.attribute_value(a.name.string(), Value()) != value)
							result = false; });
					return result;
				};

				g.node_attributes(from);

				from.for_each_sub_node([&] (Node const &node) {
					if (selected(node))
						fn(node, g);
					else
						(void)g.append_node(node, Generator::Max_depth { 20 }); });
			});
		}

		/**
		 * Remove node(s) addressed by 'query'
		 *
		 * The query describes the path to the node to modify as described
		 * for the 'replace' method. For example, the following query removes
		 * the 'rom' node named 'config' from the 'connect' node.
		 *
		 * ! remove("option | + child fb | + connect | : rom config");
		 */
		void remove(Query const &query) { replace(query, [&] (auto &&...) { }); }

		/**
		 * Append content to the node addressed by 'query'
		 */
		void append(Query const &query, auto const &fn)
		{
			_filter(query, [&] (Node const &, Node const &from, Generator &g) {
				g.node_attributes(from);
				from.for_each_sub_node([&] (Node const &node) {
					(void)g.append_node(node, Generator::Max_depth { 20 }); });
				fn(g);
			});
		}

		void with_result(auto const &fn,
		                 auto const &buffer_error_fn,
		                 auto const &alloc_error_fn) const
		{
			unsigned const src_i = _b0_to_b1 ? 0 : 1;

			_bytes[src_i].with_result(
				[&] (Allocation const &src) {
					_content.with_result(
						[&] (size_t n) {
							fn(Const_byte_range_ptr((char *)src.ptr, n)); },
						[&] (Buffer_error e) { buffer_error_fn(e); }); },
				[&] (Alloc_error e) { alloc_error_fn(e); });
		}
};

#endif /* _INCLUDE__OS__HID_EDIT_H_ */
