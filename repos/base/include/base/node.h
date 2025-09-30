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
#include <util/hrd.h>
#include <base/memory.h>

namespace Genode {
	class Node;
	class Buffered_node;
	class Generator;
	class Generated_node;
}


class Genode::Node : Noncopyable
{
	public:

		struct Attribute
		{
			using Name = String<64>;
			Name const name;
			Const_byte_range_ptr value;
		};

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

	private:

		using Xml = Xml_node;
		using Hrd = Hrd_node;

		Constructible<Hrd const &> _hrd_ref { };
		Constructible<Hrd>         _hrd     { };
		Constructible<Xml const &> _xml_ref { };
		Constructible<Xml>         _xml     { };

		auto _with(auto const &xml_fn, auto const &hrd_fn,
		           auto const &empty_fn) const -> decltype(empty_fn())
		{
			if (_hrd_ref.constructed()) return hrd_fn(*_hrd_ref);
			if (_xml_ref.constructed()) return xml_fn(*_xml_ref);
			if (_hrd.constructed())     return hrd_fn(*_hrd);
			if (_xml.constructed())     return xml_fn(*_xml);
			return empty_fn();
		}

		auto _process(auto const &empty_fn, auto const &fn) const -> decltype(empty_fn())
		{
			return _with([&] (Xml const &n) { return fn(n); },
			             [&] (Hrd const &n) { return fn(n); },
			             [&]                { return empty_fn(); });
		}

		void _process_if_valid(auto const &fn) const { _process([&] { }, fn); }

		Node(Xml const &xml) { _xml_ref.construct(xml); }
		Node(Hrd const &hrd) { _hrd_ref.construct(hrd); }

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
			      [&] (Hrd const &) { Hrd::print_quoted_line(out, bytes); },
			      [&] { });
		}

		using With_node = Callable<void, Node const &>;

		void _with_optional_sub_node(char const *, With_node::Ft const &) const;
		void _for_each_sub_node(char const *, With_node::Ft const &) const;
		void _for_each_sub_node(With_node::Ft const &) const;

		using With_attribute = Callable<void, Attribute const &>;

		void _for_each_attribute(With_attribute::Ft const &) const;

		using With_quoted_line = Callable<void, Quoted_line const &>;

		void _for_each_quoted_line(With_quoted_line::Ft const &) const;

		friend class Generator;  /* for 'Generator::append_node' */

	public:

		Node() { /* type is "empty" */ };

		Node(Const_byte_range_ptr const &bytes);

		template <size_t N>
		Node(String<N> const &s)
		: Node(Const_byte_range_ptr(s.string(), max(s.length(), 1ul) - 1ul)) { }

		/**
		 * Construct a copy of the node with the content located in 'dst'
		 */
		Node(Node const &other, Byte_range_ptr const &dst);

		void for_each_sub_node(char const *type, auto const &fn) const
		{
			_for_each_sub_node(type, With_node::Fn { fn });
		}

		void for_each_sub_node(auto const &fn) const
		{
			_for_each_sub_node(With_node::Fn { fn });
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

		unsigned num_sub_nodes() const;

		void with_optional_sub_node(char const *type, auto const &fn) const
		{
			_with_optional_sub_node(type, With_node::Fn { fn });
		}

		void for_each_attribute(auto const &fn) const
		{
			_for_each_attribute(With_attribute::Fn { fn });
		}

		template <typename T>
		T attribute_value(char const *attr, T const default_value) const
		{
			return _process([&] { return default_value; }, [&] (auto const &node) {
				return node.attribute_value(attr, default_value); });
		}

		bool has_type(char const *type) const;

		bool has_sub_node(char const *type) const;

		using Type = Xml::Type;

		Type type() const;

		bool has_attribute(char const *attr) const;

		size_t num_bytes() const;

		bool differs_from(Node const &other) const;

		void for_each_quoted_line(auto const &fn) const
		{
			_for_each_quoted_line(With_quoted_line::Fn { fn });
		}

		/**
		 * Utility for printing all quoted lines of a node
		 */
		struct Quoted_content
		{
			Node const &_node;

			void print(Output &out) const;
		};

		void print(Output &out) const;
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
			Hrd_generator *_hrd_ptr = nullptr;
		};

		Generator(Xml_generator &xml) : _xml_ptr(&xml) { }
		Generator(Hrd_generator &hrd) : _hrd_ptr(&hrd) { }

		static bool _generate_xml(); /* component-global config switch */

		void _node(char const *name, Callable<void>::Ft const &);

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
			else
				return Hrd_generator::generate(buffer, type, [&] (Hrd_generator &hrd) {
					Generator g(hrd);
					fn(g); });
		}

		void node(char const *name, auto const &fn)
		{
			_node(name, Callable<void>::Fn { fn });
		}

		void node(char const *name) { node(name, [] { }); }

		void attribute(char const *name, char const *str, size_t str_len);
		void attribute(char const *name, char const *str);
		void attribute(char const *name, bool value);
		void attribute(char const *name, long long value);
		void attribute(char const *name, unsigned long long value);
		void attribute(char const *name, double value);

		template <size_t N>
		void attribute(char const *name, String<N> const &str)
		{
			attribute(name, str.string());
		}

		void attribute(char const *name, long value)
		{
			attribute(name, static_cast<long long>(value));
		}

		void attribute(char const *name, int value)
		{
			attribute(name, static_cast<long long>(value));
		}

		void attribute(char const *name, unsigned long value)
		{
			attribute(name, static_cast<unsigned long long>(value));
		}

		void attribute(char const *name, unsigned value)
		{
			attribute(name, static_cast<unsigned long long>(value));
		}

		void append_quoted(auto &&... args)
		{
			if (_xml_ptr) _xml_ptr->append_sanitized(args...);
			if (_hrd_ptr) _hrd_ptr->append_quoted(args...);
		}

		struct Max_depth { unsigned value; };

		void node_attributes(Node const &node);

		[[nodiscard]] bool append_node(Node const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node(node, { max_depth.value });
			if (_hrd_ptr)
				return node._with(
					[&] (Xml_node const &) {
						return _hrd_ptr->append_node(node, { max_depth.value }); },
					[&] (Hrd_node const &hrd) {
						return _hrd_ptr->append_node(hrd), true; },
					[&] {
						return true; });

			return false;
		}

		[[nodiscard]] bool append_node_content(Node const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node_content(node, { max_depth.value });
			if (_hrd_ptr)
				return node._with(
					[&] (Xml_node const &) {
						return _hrd_ptr->append_node_content(node, { max_depth.value }); },
					[&] (Hrd_node const &hrd) {
						return _hrd_ptr->append_node_content(hrd), true; },
					[&] {
						return true; });

			return false;
		}

		/**
		 * Append copy of XML node
		 *
		 * \deprecated  accommodate components not fully converted to Node API yet
		 * \noapi
		 */
		[[nodiscard]] bool append_node(Xml_node const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node(node, { max_depth.value });
			if (_hrd_ptr) return _hrd_ptr->append_node(node, { max_depth.value });

			return false;
		}

		/**
		 * Append body of XML node
		 *
		 * \deprecated  accommodate components not fully converted to Node API yet
		 * \noapi
		 */
		[[nodiscard]] bool append_node_content(Xml_node const &node, Max_depth const &max_depth)
		{
			if (_xml_ptr) return _xml_ptr->append_node_content(node, { max_depth.value });
			if (_hrd_ptr) return _hrd_ptr->append_node_content(node, { max_depth.value });

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
