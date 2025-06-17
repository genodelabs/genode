/*
 * \brief  Syntax-agnostic API for parsing structured textual data
 * \author Norman Feske
 * \date   2025-06-17
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__NODE_H_
#define _INCLUDE__BASE__NODE_H_

#include <util/reconstructible.h>
#include <util/xml_node.h>
#include <base/memory.h>

namespace Genode {
	class Node;
	class Buffered_node;
}


class Genode::Node : Noncopyable
{
	private:

		Constructible<Xml_node const &> _xml_ref { };
		Constructible<Xml_node>         _xml     { };

		auto _with(auto const &xml_fn, auto const &empty_fn) const -> decltype(empty_fn())
		{
			if (_xml_ref.constructed()) return xml_fn(*_xml_ref);
			if (_xml.constructed())     return xml_fn(*_xml);
			return empty_fn();
		}

		Node(Xml_node const &xml) { _xml_ref.construct(xml); }

		static bool _looks_like_xml(char const *s, size_t n)
		{
			for (; n && is_whitespace(*s); n--, s++);
			return n && *s == '<';
		}

	public:

		Node() { /* empty */ };

		Node(Const_byte_range_ptr const &bytes)
		{
			if (_looks_like_xml(bytes.start, bytes.num_bytes))
				try { _xml.construct(bytes); } catch (...) { }
		}

		template <size_t N>
		Node(String<N> const &s)
		: Node(Const_byte_range_ptr(s.string(), max(s.length(), 1ul) - 1ul)) { }

		void for_each_sub_node(char const *type, auto const &fn) const
		{
			_with(
				[&] (Xml_node const &xml) {
					xml.for_each_sub_node([&] (Xml_node const &sub_node) {
						if (sub_node.has_type(type))
							fn(Node(sub_node)); }); },
				[&] { });
		}

		void for_each_sub_node(auto const &fn) const
		{
			_with(
				[&] (Xml_node const &xml) {
					xml.for_each_sub_node(
						[&] (Xml_node const &sub_node) { fn(Node(sub_node)); }); },
				[&] { });
		}

		auto with_sub_node(char const *type, auto const &fn, auto const &missing_fn) const
		-> decltype(missing_fn())
		{
			return _with(
				[&] (Xml_node const &xml) {
					return xml.with_sub_node(type,
						[&] (Xml_node const &sub_node) { return fn(Node(sub_node)); },
						[&]                            { return missing_fn(); });
				},
				[&] { return missing_fn(); });
		}

		unsigned num_sub_nodes() const
		{
			unsigned count = 0;
			for_each_sub_node([&] (auto const &) { count++; });
			return count;
		}

		void with_optional_sub_node(char const *type, auto const &fn) const
		{
			_with(
				[&] (Xml_node const &xml) {
					xml.with_sub_node(type,
						[&] (Xml_node const &sub_node) { fn(Node(sub_node)); },
						[&] { }); },
				[&] { });
		}

		struct Attribute
		{
			using Name = String<64>;
			Name const name;
			Const_byte_range_ptr value;
		};

		void for_each_attribute(auto const &fn) const
		{
			_with(
				[&] (Xml_node const &xml) {
					xml.for_each_attribute([&] (Xml_attribute const &a) {
						a.with_raw_value([&] (char const *start, size_t len) {
							fn(Attribute { a.name(), { start, len } }); }); }); },
				[&] { });
		}

		template <typename T>
		T attribute_value(char const *attr, T const default_value) const
		{
			return _with(
				[&] (Xml_node const &xml) { return xml.attribute_value(attr, default_value); },
				[&]                       { return default_value; });
		}

		bool has_type(char const *type) const
		{
			return _with([&] (Xml_node const &xml) { return xml.has_type(type); },
			             [&]                       { return Type { "empty" } == type; });
		}

		bool has_sub_node(char const *type) const
		{
			bool result = false;
			with_optional_sub_node(type, [&] (Node const &) { result = true; });
			return result;
		}

		using Type = Xml_node::Type;

		Type type() const
		{
			return _with([&] (Xml_node const &xml) { return xml.type(); },
			             []                        { return Type { "empty" }; });
		}

		bool has_attribute(char const *attr) const
		{
			return _with([&] (Xml_node const &xml) { return xml.has_attribute(attr); },
			             []                        { return false; });
		}

		void with_raw_node(auto const &fn) const
		{
			_with([&] (Xml_node const &xml) { xml.with_raw_node(fn); },
			      [&] { });
		}

		size_t num_bytes() const
		{
			return _with([&] (Xml_node const &xml) { return xml.size(); },
			             [&] () -> size_t          { return 0; });
		}

		bool differs_from(Node const &other) const
		{
			return _with(
				[&] (Xml_node const &xml) {
					return other._with(
						[&] (Xml_node const &other_xml) { return xml.differs_from(other_xml); },
						[]                              { return true; }); },
				[&] {
					return other._with(
						[&] (Xml_node const &) { return true; },
						[]                     { return false; });
			});
		}

		template <typename STRING>
		STRING decoded_content() const
		{
			return _with([&] (Xml_node const &xml) { return xml.decoded_content<STRING>(); },
			             [&]                       { return STRING(); });
		}

		void print(Output &out) const
		{
			_with([&] (Xml_node const &xml) { xml.print(out); },
			      [&] { });
		}
};


struct Genode::Buffered_node : private Memory::Allocation::Attempt, Node
{
	static Const_byte_range_ptr _copy(Memory::Constrained_allocator &alloc,
	                                  Memory::Allocation::Attempt &a,
	                                  Node const &node)
	{
		char const *node_start = nullptr;
		size_t      node_num_bytes = 0;

		node.with_raw_node([&] (char const *start, size_t num_bytes) {
			a = alloc.try_alloc(num_bytes);
			a.with_result([&] (Memory::Allocation &a) {
				node_num_bytes = min(num_bytes, a.num_bytes);
				node_start     = (char const *)a.ptr;
				Genode::memcpy(a.ptr, start, node_num_bytes);
			}, [&] (Memory::Allocation::Error) { });
		});

		return { node_start, node_num_bytes };
	}

	Buffered_node(Memory::Constrained_allocator &alloc, Node const &node)
	:
		Memory::Allocation::Attempt(Alloc_error::DENIED),
		Node(_copy(alloc, *this, node))
	{ }
};

#endif /* _INCLUDE__BASE__NODE_H_ */
