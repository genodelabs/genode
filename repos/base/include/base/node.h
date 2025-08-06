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
#include <util/xml_generator.h>
#include <base/memory.h>

namespace Genode {
	class Node;
	class Buffered_node;
	class Generator;
	class Generated_node;
}


class Genode::Node : Noncopyable
{
	private:

		using Xml = Xml_node;

		Constructible<Xml const &> _xml_ref { };
		Constructible<Xml>         _xml     { };

		auto _with(auto const &xml_fn,
		           auto const &empty_fn) const -> decltype(empty_fn())
		{
			if (_xml_ref.constructed()) return xml_fn(*_xml_ref);
			if (_xml.constructed())     return xml_fn(*_xml);
			return empty_fn();
		}

		auto _process(auto const &empty_fn, auto const &fn) const -> decltype(empty_fn())
		{
			return _with([&] (Xml const &n) { return fn(n); },
			             [&]                { return empty_fn(); });
		}

		void _process_if_valid(auto const &fn) const { _process([&] { }, fn); }

		Node(Xml const &xml) { _xml_ref.construct(xml); }

		static void _with_skipped_whitespace(auto const &bytes, auto const &fn)
		{
			char const *s = bytes.start;
			size_t      n = bytes.num_bytes;

			for (; n && is_whitespace(*s); n--, s++);

			if (n) fn(Const_byte_range_ptr(s, n));
		}

		void _print_quoted_line(Output &out, Const_byte_range_ptr const &bytes) const
		{
			_with([&] (Xml const &) { Xml::print_quoted_line(out, bytes); },
			      [&] { });
		}

	public:

		Node() { /* type is "empty" */ };

		Node(Const_byte_range_ptr const &bytes)
		{
			_with_skipped_whitespace(bytes, [&] (Const_byte_range_ptr const &bytes) {
				if (bytes.start[0] == '<') {
					try { _xml.construct(bytes); } catch (...) { }
				}
			});
		}

		template <size_t N>
		Node(String<N> const &s)
		: Node(Const_byte_range_ptr(s.string(), max(s.length(), 1ul) - 1ul)) { }

		/**
		 * Construct a copy of the node with the content located in 'dst'
		 */
		Node(Node const &other, Byte_range_ptr const &dst)
		{
			other._with(
				[&] (Xml const &node) {
					node.with_raw_node([&] (char const *start, size_t num_bytes) {
						if (dst.num_bytes >= num_bytes) {
							memcpy(dst.start, start, num_bytes);
							_xml.construct(dst.start, num_bytes); } });
				},
				[&]                   { });
		}

		void for_each_sub_node(char const *type, auto const &fn) const
		{
			_process_if_valid([&] (auto const &node) {
				node.for_each_sub_node([&] (auto const &sub_node) {
					if (sub_node.type() == type)
						fn(Node(sub_node)); }); });
		}

		void for_each_sub_node(auto const &fn) const
		{
			_process_if_valid([&] (auto const &node) {
				node.for_each_sub_node([&] (auto const &sub_node) {
					fn(Node(sub_node)); }); });
		}

		auto with_sub_node(char const *type, auto const &fn, auto const &missing_fn) const
		-> decltype(missing_fn())
		{
			return _process(missing_fn, [&] (auto const &node) {
				return node.with_sub_node(type,
					[&] (auto const &sub_node) { return fn(Node(sub_node)); },
					[&]                        { return missing_fn(); }); });
		}

		auto with_sub_node(unsigned n, auto const &fn, auto const &missing_fn) const
		-> decltype(missing_fn())
		{
			return _process(missing_fn, [&] (auto const &node) {
				return node.with_sub_node(n,
					[&] (auto const &sub_node) { return fn(Node(sub_node)); },
					[&]                        { return missing_fn(); }); });
		}

		unsigned num_sub_nodes() const
		{
			unsigned count = 0;
			for_each_sub_node([&] (auto const &) { count++; });
			return count;
		}

		void with_optional_sub_node(char const *type, auto const &fn) const
		{
			_process_if_valid([&] (auto const &node) {
				node.with_sub_node(type,
					[&] (auto const &sub_node) { fn(Node(sub_node)); },
					[&]                        { }); });
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
				[&] (Xml const &n) {
					n.for_each_attribute([&] (Xml_attribute const &a) {
						a.with_raw_value([&] (char const *start, size_t len) {
							fn(Attribute { a.name(), { start, len } }); }); }); },
				[&] { });
		}

		template <typename T>
		T attribute_value(char const *attr, T const default_value) const
		{
			return _process([&] { return default_value; }, [&] (auto const &node) {
				return node.attribute_value(attr, default_value); });
		}

		bool has_type(char const *type) const
		{
			return _process([&] { return Type { "empty" } == type; },
			                [&] (auto const &node) { return node.has_type(type); });
		}

		bool has_sub_node(char const *type) const
		{
			bool result = false;
			with_optional_sub_node(type, [&] (Node const &) { result = true; });
			return result;
		}

		using Type = Xml::Type;

		Type type() const
		{
			return _process([] { return Type { "empty" }; }, [&] (auto const &node) {
				return node.type(); });
		}

		bool has_attribute(char const *attr) const
		{
			return _process([] { return false; }, [&] (auto const &node) {
				return node.has_attribute(attr); });
		}

		size_t num_bytes() const
		{
			return _with([&] (Xml const &n) { return n.size(); },
			             [&] () -> size_t   { return 0; });
		}

		bool differs_from(Node const &other) const
		{
			return _with(
				[&] (Xml const &n) {
					return other._with(
						[&] (Xml const &other_n) { return n.differs_from(other_n); },
						[]                       { return true; }); },
				[&] {
					return other._with(
						[&] (Xml const &) { return true; },
						[]                { return false; });
			});
		}

		class Quoted_line
		{
			private:

				Node const &_node;
				Const_byte_range_ptr _bytes;

				friend class Node;

				Quoted_line(Node const &node, char const *start, size_t len, bool last)
				: _node(node), _bytes(start, len), last(last) { }

			public:

				bool const last;

				void print(Output &out) const { _node._print_quoted_line(out, _bytes); }
		};

		void for_each_quoted_line(auto const &fn) const
		{
			_process_if_valid([&] (auto const &node) {
				node.for_each_quoted_line([&] (auto const &l) {
					fn(Quoted_line { *this, l.bytes.start, l.bytes.num_bytes,
					                        l.last }); }); });
		}

		/**
		 * Utility for printing all quoted lines of a node
		 */
		struct Quoted_content
		{
			Node const &_node;

			void print(Output &out) const
			{
				_node.for_each_quoted_line([&] (auto const &line) {
					line.print(out);
					if (!line.last) out.out_char('\n'); });
			}
		};

		void print(Output &out) const
		{
			_process_if_valid([&] (auto const &node) { node.print(out); });
		}
};


struct Genode::Buffered_node : private Memory::Allocation::Attempt, Node
{
	static Byte_range_ptr _allocated(Memory::Constrained_allocator &alloc,
	                                 Memory::Allocation::Attempt &a, size_t num_bytes)
	{
		if (!num_bytes)
			return { nullptr, 0 };

		a = alloc.try_alloc(num_bytes);
		return a.convert<Byte_range_ptr>(
			[&] (Memory::Allocation &a) {
				return Byte_range_ptr((char *)a.ptr, a.num_bytes); },
			[&] (Alloc_error) {
				return Byte_range_ptr(nullptr, 0); });
	}

	Buffered_node(Memory::Constrained_allocator &alloc, Node const &node)
	:
		Memory::Allocation::Attempt(Alloc_error::DENIED),
		Node(node, _allocated(alloc, *this, node.num_bytes()))
	{ }
};


class Genode::Generator : Noncopyable
{
	private:

		struct {
			Xml_generator *_xml_ptr = nullptr;
		};

		Generator(Xml_generator &xml) : _xml_ptr(&xml) { }

		static bool _generate_xml(); /* component-global config switch */

	public:

		using Result = Attempt<size_t, Buffer_error>;
		using Type   = String<64>;

		/**
		 * Fill buffer with with textual data generated by 'fn'
		 */
		static Result generate(Byte_range_ptr const &buffer,
		                       Type const &type, auto const &fn)
		{
			if (_generate_xml())
				return Xml_generator::generate(buffer, type, [&] (Xml_generator &xml) {
					Generator g(xml);
					fn(g); });
			return 0ul;
		}

		void node(auto &&... args)
		{
			if (_xml_ptr) _xml_ptr->node(args...);
		}

		void attribute(auto &&... args)
		{
			if (_xml_ptr) _xml_ptr->attribute(args...);
		}

		void append_quoted(auto &&... args)
		{
			if (_xml_ptr) _xml_ptr->append_sanitized(args...);
		}

		struct Max_depth { unsigned value; };

		void node_attributes(Node const &node)
		{
			if (_xml_ptr) _xml_ptr->node_attributes(node);
		}

		[[nodiscard]] bool append_node(auto const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node(node, { max_depth.value });

			return false;
		}

		[[nodiscard]] bool append_node_content(auto const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node_content(node, { max_depth.value });

			return false;
		}
};


struct Genode::Generated_node
{
	Memory::Allocation::Attempt const allocation;

	using Result = Unique_attempt<Node, Buffer_error>;

	Result const node;

	Result _generate(Generator::Type const &type, auto const &fn)
	{
		return allocation.convert<Result>(
			[&] (Memory::Allocation const &a) {
				Byte_range_ptr bytes((char *)a.ptr, a.num_bytes);
				return Generator::generate(bytes, type, fn).template convert<Result>(
					[&] (size_t n) -> Result {
						return { Const_byte_range_ptr(bytes.start, n) };
					},
					[&] (Buffer_error e) -> Result { return e; });
			},
			[&] (Alloc_error) { return Buffer_error::EXCEEDED; });
	}

	Generated_node(Memory::Constrained_allocator &alloc, size_t num_bytes,
	               Generator::Type const &type, auto const &fn)
	:
		allocation(alloc.try_alloc(num_bytes)),
		node(_generate(type, fn))
	{ }
};

#endif /* _INCLUDE__BASE__NODE_H_ */
