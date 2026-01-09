/*
 * \brief  Parser and generator for human-inclined data (HID)
 * \author Norman Feske
 * \date   2025-06-11
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__HID_H_
#define _INCLUDE__UTIL__HID_H_

#include <util/string.h>
#include <util/attempt.h>
#include <util/noncopyable.h>
#include <util/callable.h>
#include <base/log.h>

namespace Genode {
	class Hid_node;
	class Hid_generator;
	class Xml_node; /* forward declaration */
}


class Genode::Hid_node : Noncopyable
{
	public:

		using Type = String<64>;

		struct Attribute { Const_byte_range_ptr tag, value; };

	private:

		struct Indent { unsigned value; };

		static bool _letter           (char c) { return c >= 'a' && c <= 'z'; }
		static bool _digit            (char c) { return c >= '0' && c <= '9'; }
		static bool _underscore       (char c) { return c == '_'; }
		static bool _space            (char c) { return c == ' '; }
		static bool _minus            (char c) { return c == '-'; }
		static bool _newline          (char c) { return c == '\n'; }
		static bool _space_or_newline (char c) { return c == ' ' || _newline(c); }

		using Span = Const_byte_range_ptr;

		static void _with_split_at(Span const &bytes, unsigned const n, auto const &fn)
		{
			if (n <= bytes.num_bytes)
				fn(Span(bytes.start, n), Span(bytes.start + n, bytes.num_bytes - n));
		}

		static void _with_ident(Span const &bytes, auto const &fn)
		{
			unsigned n = 0;
			for (; n < bytes.num_bytes; n++) {
				char const c = bytes.start[n];

				if (n == 0 && !_letter(c))
					break;

				if (!_letter(c) && !_digit(c) && !_underscore(c) && !_minus(c))
					break;
			}
			if (n)
				_with_split_at(bytes, n, fn);
		}

		static void _with_trimmed(Span const &bytes, auto const &fn)
		{
			char const *s = bytes.start;
			size_t      n = bytes.num_bytes;
			for (; n && _space(s[0]); s++, n--);
			for (; n && _space(s[n - 1]); n--);
			fn(Span(s, n));
		}

		static unsigned _num_spaces(Span const &bytes)
		{
			unsigned n = 0;
			while (n < bytes.num_bytes && _space(bytes.start[n]))
				n++;
			return n;
		}

		static void _with_skipped(Span const &bytes, unsigned const n, auto const &fn)
		{
			if (bytes.num_bytes >= n)
				fn(Span { bytes.start + n, bytes.num_bytes - n });
		}

		static void _with_type(Span const &bytes, auto const &fn)
		{
			bool ok = false;
			_with_ident(bytes, [&] (Span const &ident, Span const &) {
				fn(ident);
				ok = true; });

			if (!ok) fn(Span(nullptr, 0));
		}

		struct Prefix
		{
			enum Type { INVALID, EMPTY, TOP, NODE, XNODE, COMMENT, RAW, OTHER } type;

			static Prefix from_char(char c)
			{
				switch (c) {
				case '+': return { NODE };
				case 'x': return { XNODE };
				case '.': return { COMMENT };
				case ':': return { RAW };
				default:  return { INVALID };
				}
			};

			bool valid()          const { return type != INVALID; };
			bool node()           const { return type == NODE; };
			bool node_or_xnode()  const { return type == NODE || type == XNODE; };
			bool line_delimited() const { return type == COMMENT || type == RAW; }
		};

		struct At { bool first; /* first segment of a line */ };

		/*
		 * Utilities
		 */

		static void _with_segment(At at, Span const &bytes, auto const &fn)
		{
			/* empty line */
			if (bytes.num_bytes == 0) {
				fn({ Prefix::EMPTY }, Span(nullptr, 0), Span(nullptr, 0));
				return;
			}

			/* top-level node */
			if (at.first && _letter(bytes.start[0])) {
				bytes.cut('|', [&] (Span const &head, Span const &tail) {
					fn({ Prefix::TOP }, head, tail); });
				return;
			}

			_with_skipped(bytes, _num_spaces(bytes), [&] (Span const &bytes) {

				/* line prefix followed by one space */
				if (bytes.num_bytes > 1 && _space(bytes.start[1])) {

					Prefix const prefix = Prefix::from_char(bytes.start[0]);

					if (prefix.valid()) {
						_with_skipped(bytes, 2, [&] (Span const &remain) {
							if (prefix.line_delimited())
								fn(prefix, remain, Span(nullptr, 0));
							else
								remain.cut('|', [&] (Span const &head, Span const &tail) {
									fn(prefix, head, tail); });
						});
						return;
					}
				}

				/* other content, i.e., attribute */
				bytes.cut('|', [&] (Span const &head, Span const &tail) {
					fn({ Prefix::OTHER }, head, tail); });
			});
		}

		static void _for_each_segment(Span const &bytes, auto const &fn)
		{
			bool first = true;
			bytes.split('\n', [&] (Span const &line) {
				char const *start = line.start;
				size_t  num_bytes = line.num_bytes;

				/* tolerate CRLF line endings */
				if (num_bytes && line.start[num_bytes - 1] == '\r') num_bytes--;

				while (num_bytes)
					_with_segment({ .first = first }, { start, num_bytes },
						[&] (Prefix prefix, Span const &seg, Span const &remain) {
							if (seg.num_bytes && prefix.type != Prefix::COMMENT)
								fn(prefix, Indent { unsigned(seg.start - line.start) }, seg);
							start     = remain.start;
							num_bytes = remain.num_bytes;
							first     = false;
						});
			});
		}

		/* let '_for_each_sub_node' be a regular function, not a template */
		using With_indent_span = Callable<void, Indent const &, Span const &>;

		static void _for_each_sub_node(Span const &, With_indent_span::Ft const &);

		static inline void _for_each_attr(Span const &, auto const &);

		using With_attribute = Callable<void, Attribute const &>;

		void _for_each_attribute(With_attribute::Ft const &fn) const;

		static inline Const_byte_range_ptr _validated(Const_byte_range_ptr const &);

		/*
		 * Member variables and methods
		 */

		Const_byte_range_ptr const _bytes;

		Indent const _indent { 0 };

		friend class Hid_generator;  /* for 'Hid_generator::_copy' */

		auto _with_sub_node(auto const &match_fn, auto const &fn,
		                    auto const &missing_fn) const -> decltype(missing_fn())
		{
			char const *start     { };
			size_t      num_bytes { };
			Indent      indent    { };

			for_each_sub_node([&] (Hid_node const &node) {
				if (!start && match_fn(node)) {
					start     = node._bytes.start;
					num_bytes = node._bytes.num_bytes;
					indent    = node._indent; } });

			if (!start)
				return missing_fn();

			return fn(Hid_node { indent, { start, num_bytes } });
		}

		using With_tag_value = Callable<void, Span const &, Span const &>;

		void _with_tag_value(char const *type, With_tag_value::Ft const &fn) const;

		Const_byte_range_ptr _copied(Byte_range_ptr const &dst) const
		{
			if (dst.num_bytes < _bytes.num_bytes)
				return { nullptr, 0 };

			memcpy(dst.start, _bytes.start, _bytes.num_bytes);
			return { dst.start, _bytes.num_bytes };
		}

		Hid_node(Indent i, Span const &s) : _bytes(s.start, s.num_bytes), _indent(i) { }

	public:

		Hid_node(Const_byte_range_ptr const &);

		Hid_node(Hid_node const &other, Byte_range_ptr const &dst)
		:
			Hid_node(other._indent, other._copied(dst))
		{ }

		bool valid() const { return _bytes.num_bytes > 0; }

		Type type() const
		{
			Type result { "invalid" };

			_with_type(_bytes, [&] (Span const &type) {
				if (type.num_bytes)
					result = { Cstring(type.start, type.num_bytes) }; });

			return result;
		}

		bool has_type(char const *t) const { return type() == t; }

		void for_each_attribute(auto const &fn) const
		{
			_for_each_attribute(With_attribute::Fn { fn });
		}

		unsigned num_sub_nodes() const
		{
			unsigned n = 0;
			_for_each_sub_node(_bytes, With_indent_span::Fn {
				[&] (Indent const, Span const &) { n++; } });
			return n;
		}

		void for_each_sub_node(auto const &fn) const
		{
			_for_each_sub_node(_bytes, With_indent_span::Fn {
				[&] (Indent const indent, Span const &s) {
					fn(Hid_node { indent, s }); } });
		}

		auto with_sub_node(char const *type, auto const &fn,
		                   auto const &missing_fn) const -> decltype(missing_fn())
		{
			unsigned found = false;

			auto match = [&] (Hid_node const &node)
			{
				if (found || node.type() != type)
					return false;

				found = true;
				return true;
			};

			return _with_sub_node(match, fn, missing_fn);
		}

		auto with_sub_node(unsigned n, auto const &fn,
		                   auto const &missing_fn) const -> decltype(missing_fn())
		{
			unsigned count = 0;
			return _with_sub_node([&] (Hid_node const &) { return count++ == n; },
			                      fn, missing_fn);
		}

		template <typename T>
		T attribute_value(char const *type, T const default_value) const;

		template <size_t N>
		String<N> attribute_value(char const *type, String<N> const default_value) const;

		bool has_attribute(char const *type) const
		{
			bool result = false;
			_with_tag_value(type, With_tag_value::Fn {[&] (auto &, auto &) { result = true; } });
			return result;
		}

		static void print_quoted_line(Output &out, Const_byte_range_ptr const &bytes)
		{
			Genode::print(out, Cstring(bytes.start, bytes.num_bytes));
		}

		struct Quoted_line
		{
			Const_byte_range_ptr bytes;
			bool const last;
			void print(Output &out) const { print_quoted_line(out, bytes); }
		};

		void for_each_quoted_line(auto const &fn) const
		{
			if (num_sub_nodes()) /* quoted lines cannot appear besides sub nodes */
				return;

			char const *start     { }; /* visited line */
			size_t      num_bytes { };

			_for_each_segment(_bytes,
				[&] (Prefix const  prefix, Indent const, Span const &seg) {
					if (prefix.type != Prefix::RAW)
						return;

					if (start)
						fn(Quoted_line { .bytes = { start, num_bytes }, .last = false });

					start     = seg.start;
					num_bytes = seg.num_bytes;
				});

			if (start)
				fn(Quoted_line { .bytes = { start, num_bytes }, .last = true });
		}

		size_t num_bytes() const { return _bytes.num_bytes; }

		bool differs_from(Hid_node const &other) const { return !_bytes.equals(other._bytes); }

		void print(Output &out) const { out.out_string(_bytes.start, _bytes.num_bytes); }
};


template <typename T>
T Genode::Hid_node::attribute_value(char const *type, T const default_value) const
{
	T result = default_value;
	_with_tag_value(type, With_tag_value::Fn {
		[&] (Span const &, Span const &value) {
			if (!value.num_bytes || (value.num_bytes != parse(value, result)))
				result = default_value; } });
	return result;
}


template <Genode::size_t N>
Genode::String<N>
Genode::Hid_node::attribute_value(char const *type, String<N> const default_value) const
{
	String<N> result = default_value;
	_with_tag_value(type, With_tag_value::Fn {
		[&] (Span const &, Span const &value) {
			result = { Cstring(value.start, value.num_bytes) }; } });
	return result;
}


class Genode::Hid_generator : Noncopyable
{
	private:

		struct Out_buffer : Output
		{
			Byte_range_ptr _bytes;

			size_t _used     = 0;
			bool   _exceeded = false;

			/*
			 * Return true if 'len' chars fit into the buffer
			 */
			[[nodiscard]] bool _fits(size_t const len) const
			{
				return _used + len <= _bytes.num_bytes;
			}

			void _append_char(char c)
			{
				if (_fits(1))
					_bytes.start[_used++] = c;
				else
					_exceeded = true;
			}

			Out_buffer(Byte_range_ptr const &bytes) : _bytes(bytes.start, bytes.num_bytes) { }

			bool   exceeded() const { return _exceeded; }
			size_t used()     const { return _used; }

			void rewind(size_t used) { _used = min(_used, used); }

			void out_char(char c) override { _append_char(c); }

			void out_string(char const *str, size_t n) override
			{
				for (unsigned i = 0; i < n && !_exceeded && str[i]; i++)
					_append_char(str[i]);
			}

			void with_inserted_gap(size_t const at, size_t const len, auto const &fn)
			{
				/* don't allow the insertion into non-populated part */
				if (at > _used || !_fits(len)) {
					_exceeded = true;
					return;
				}

				memmove(_bytes.start + at + len, _bytes.start + at, _used - at);

				_used += len;

				Out_buffer gap { { _bytes.start + at, len } };
				fn(gap);
			}
		};

		struct Indent
		{
			unsigned level;

			void print(Output &out) const
			{
				for (unsigned i = 2; i < level*2; i++) out.out_char(' ');
			}
		};

		Out_buffer _out_buffer;

		struct Node_state
		{
			Indent indent;
			size_t attr_offset;
			bool   has_attr;

			struct Quote { bool started, line_used; } quote;
		};

		Node_state _node_state { };

		struct Tabular;

		struct { Tabular *_tabular_ptr = nullptr; };

		void _attribute(char const *, char const *, size_t);

		Hid_generator(Byte_range_ptr const &bytes, char const *name, auto const &fn)
		:
			_out_buffer(bytes)
		{
			node(name, fn);
		}

		using Node_fn = Callable<void>;

		void _print_node_type(Span const &);
		void _node(char const *, Node_fn::Ft const &);
		void _copy(Hid_node const &);
		void _start_quoted_line();
		void _append_quoted(Span const &);

		[[nodiscard]] bool _try_append_quoted(auto const &node)
		{
			bool quoted = false;
			node.for_each_quoted_line([&] (auto const &line) {
				quoted = true;
				print(_out_buffer, "\n", _node_state.indent, ": ", line);
			});
			return quoted;
		}

		void _tabular(Node_fn::Ft const &);

	public:

		using Result   = Attempt<size_t, Buffer_error>;
		using Tag_name = String<64>;

		/**
		 * Fill buffer with content generated by 'fn'
		 */
		static Result generate(Byte_range_ptr const &buffer,
		                       Tag_name       const &tag, auto const &fn)
		{
			Hid_generator hid(buffer, tag.string(), [&] { fn(hid); });

			if (hid._out_buffer.exceeded())
				return Buffer_error::EXCEEDED;

			return hid._out_buffer.used();
		}

		void node(char const *name, auto const &fn)
		{
			_node(name, Node_fn::Fn { fn });
		}

		void node(char const *name) { node(name, [] { }); }

		void tabular(auto const &fn) { _tabular(Node_fn::Fn { fn }); }

		void attribute(char const *name, char const *str, size_t str_len)
		{
			_attribute(name, str, str_len);
		}

		void attribute(char const *name, char const *str)
		{
			attribute(name, str, strlen(str));
		}

		template <size_t N>
		void attribute(char const *name, String<N> const &str)
		{
			attribute(name, str.string());
		}

		void attribute(char const *name, bool value)
		{
			attribute(name, value ? "true" : "false");
		}

		void attribute(char const *name, long long value)
		{
			attribute(name, String<64>(value));
		}

		void attribute(char const *name, long value)
		{
			attribute(name, static_cast<long long>(value));
		}

		void attribute(char const *name, int value)
		{
			attribute(name, static_cast<long long>(value));
		}

		void attribute(char const *name, unsigned long long value)
		{
			attribute(name, String<64>(value));
		}

		void attribute(char const *name, unsigned long value)
		{
			attribute(name, static_cast<unsigned long long>(value));
		}

		void attribute(char const *name, unsigned value)
		{
			attribute(name, static_cast<unsigned long long>(value));
		}

		void attribute(char const *name, double value)
		{
			attribute(name, String<64>(value));
		}

		/**
		 * Append quoted content to HID node
		 *
		 * This method must not be followed by calls of 'attribute'.
		 */
		void append_quoted(char const *s, size_t len) { _append_quoted({ s, len }); }

		void append_quoted(char const *str) { append_quoted(str, strlen(str)); }

		template <size_t N>
		void append_quoted(String<N> const &s)
		{
			s.with_span([&] (Span const &s) { append_quoted(s.start, s.num_bytes); });
		}

		/**
		 * Copy all attributes from the given node
		 */
		void node_attributes(auto const &node)
		{
			node.for_each_attribute([&] (auto const &attr) {
				attribute(attr.name.string(),
				          attr.value.start, attr.value.num_bytes); });
		}

		/**
		 * Compatibility helper for the transition from Xml_node to Node
		 *
		 * As the interface of 'Xml_attribute' differs from 'Attribute', the
		 * 'node_attributes' template above cannot be used for 'Xml_node'.
		 */
		void node_attributes(Xml_node const &node);

		struct Max_depth { unsigned value; };

		/**
		 * Append content of node
		 *
		 * The content can either be quoted content or sub nodes but not a mix
		 * of both.
		 *
		 * \return false if node structure exceeds 'max_depth'
		 */
		[[nodiscard]] bool append_node_content(auto const &node, Max_depth max_depth)
		{
			if (!max_depth.value)
				return false;

			if (_try_append_quoted(node))
				return true;

			bool ok = true;
			node.for_each_sub_node([&] (auto const &sub_node) {
				if (ok)
					ok = append_node(sub_node, { max_depth.value - 1 }); });
			return ok;
		}

		/**
		 * Append content of HID node
		 *
		 * The content can either be quoted content or sub nodes but not a mix
		 * of both.
		 */
		void append_node_content(Hid_node const &node)
		{
			if (_try_append_quoted(node))
				return;

			node.for_each_sub_node([&] (Hid_node const &sub_node) {
				append_node(sub_node); });
		}

		/**
		 * Append a copy of node
		 *
		 * \return false if node structure exceeds 'max_depth'
		 */
		[[nodiscard]] bool append_node(auto const &node, Max_depth max_depth)
		{
			bool result = true;
			this->node(node.type().string(), [&] {
				node_attributes(node);
				result = append_node_content(node, max_depth);
			});
			return result;
		}

		/**
		 * Append a verbatim copy of an HID node
		 */
		void append_node(Hid_node const &node) { _copy(node); }
};

#endif /* _INCLUDE__UTIL__HID_H_ */
