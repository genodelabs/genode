/*
 * \brief  Utility for generating XML
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__XML_GENERATOR_H_
#define _INCLUDE__UTIL__XML_GENERATOR_H_

#include <util/string.h>
#include <util/callable.h>
#include <util/attempt.h>
#include <util/print_lines.h>

namespace Genode { class Xml_node; /* forward declaration */ }
namespace Genode { class Xml_generator; }


class Genode::Xml_generator
{
	private:

		class Out_buffer
		{
			private:

				char  *_dst      = nullptr;
				size_t _capacity = 0;
				size_t _used     = 0;

				/*
				 * Return true if adding 'len' chars would exhaust the buffer
				 */
				[[nodiscard]] bool _exhausted(size_t const len) const
				{
					return _used + len > _capacity;
				}

			public:

				struct [[nodiscard]] Result { bool exceeded; };

				Out_buffer(char *dst, size_t capacity)
				: _dst(dst), _capacity(capacity) { }

				static Out_buffer invalid() { return { nullptr, 0 }; }

				bool valid() const { return _dst != nullptr; }

				Result advance(size_t const len)
				{
					if (_exhausted(len))
						return { .exceeded = true };

					_used += len;
					return { };
				}

				void undo_append(size_t const len)
				{
					_used = len < _used ? _used - len : 0;
				}

				/**
				 * Append character
				 */
				Result append(char const c)
				{
					if (_exhausted(1))
						return { .exceeded = true };

					_dst[_used] = c;
					return advance(1);
				}

				/**
				 * Append character 'n' times
				 */
				Result append(char const c, size_t n)
				{
					while (n--)
						if (append(c).exceeded)
							return { .exceeded = true };
					return { };
				}

				/**
				 * Append character buffer
				 */
				Result append(char const *src, size_t len)
				{
					while (len--)
						if (append(*src++).exceeded)
							return { .exceeded = true };
					return { };
				}

				/**
				 * Append null-terminated string
				 */
				Result append(char const *src) { return append(src, strlen(src)); }

				/**
				 * Append character, sanitize it if needed
				 */
				Result append_sanitized(char const c)
				{
					switch (c) {
					case 0:    return append("&#x00;");
					case '>':  return append("&gt;");
					case '<':  return append("&lt;");
					case '&':  return append("&amp;");
					case '"':  return append("&quot;");
					case '\'': return append("&apos;");
					default:   return append(c);
					}
					return { };
				}

				/**
				 * Append character buffer, sanitize characters if needed
				 */
				Result append_sanitized(char const *src, size_t len)
				{
					while (len--)
						if (append_sanitized(*src++).exceeded)
							return { .exceeded = true };
					return { };
				}

				/**
				 * Return unused part of the buffer
				 */
				Out_buffer remainder() const
				{
					return Out_buffer(_dst + _used, _capacity - _used);
				}

				/**
				 * Insert gap into already populated part of the buffer
				 *
				 * \return  empty buffer spanning the gap
				 */
				Out_buffer insert_gap(size_t const at, size_t const len)
				{
					/* don't allow the insertion into non-populated part */
					if (at > _used)
						return Out_buffer::invalid();

					if (_exhausted(len))
						return Out_buffer::invalid();

					memmove(_dst + at + len, _dst + at, _used - at);

					if (advance(len).exceeded)
						return Out_buffer::invalid();

					return { _dst + at, len };
				}

				bool has_trailing_newline() const
				{
					return (_used > 1) && (_dst[_used - 1] == '\n');
				}

				/**
				 * Return number of unused bytes of the buffer
				 */
				size_t used() const { return _used; }

				void discard_trailing_whitespace()
				{
					for (; _used > 0 && is_whitespace(_dst[_used - 1]); _used--);
				}
		};


		class Node
		{
			public:

				using Result = Out_buffer::Result;

			private:

				/**
				 * Indentation level of node
				 */
				unsigned const _indent_level;

				Node * const _parent_node = 0;
				bool   const _parent_was_indented;
				bool   const _parent_had_content;

				Out_buffer _out_buffer;

				bool _has_content = false;
				bool _is_indented = false;
				bool _exceeded    = false;

				/**
				 * Cursor position of next attribute to insert
				 */
				size_t _attr_offset = 0;

				/**
				 * Called by sub node
				 *
				 * \param indented  if true, the returned buffer and the
				 *                  end tag start at a fresh line. This is the
				 *                  case if the content buffer contains sub
				 *                  nodes. But when inserting raw content,
				 *                  such additional whitespaces are not desired.
				 */
				Out_buffer _content_buffer(bool indented)
				{
					if (!_has_content)
						if (_out_buffer.append(">").exceeded)
							return Out_buffer::invalid();

					if (indented)
						if (_out_buffer.append("\n").exceeded)
							return Out_buffer::invalid();

					_has_content = true;
					_is_indented = indented;

					return _out_buffer.remainder();
				}

				void _undo_content_buffer(bool indented,
				                          bool was_indented,
				                          bool had_content)
				{
					_is_indented = was_indented;
					_has_content = had_content;

					if (indented)
						_out_buffer.undo_append(1);

					if (!_has_content)
						_out_buffer.undo_append(1);
				}

				/**
				 * Called by sub node
				 */
				Out_buffer::Result _commit_content(Out_buffer content_buffer)
				{
					return _out_buffer.advance(content_buffer.used());
				}

				Node(Xml_generator &, char const *, bool, Callable<void>::Ft const &);

				Result _with_out_buffer(auto const &fn)
				{
					Out_buffer dst = _content_buffer(false);

					return { .exceeded = !dst.valid()
					                   || fn(dst).exceeded
					                   || _commit_content(dst).exceeded };
				}

				void _on_exception(Xml_generator &);

			public:

				Result insert_attribute(char const *name, char const *value, size_t val_len)
				{
					/* ' ' + name + '=' + '"' + value + '"' */
					size_t const gap = 1 + strlen(name) + 1 + 1 + val_len + 1;

					Out_buffer dst = _out_buffer.insert_gap(_attr_offset, gap);

					Result const result {
						.exceeded = !dst.valid()
						          || dst.append(' ')  .exceeded
						          || dst.append(name) .exceeded
						          || dst.append("=\"").exceeded
						          || dst.append(value, val_len).exceeded
						          || dst.append("\"") .exceeded
					};

					if (!result.exceeded)
						_attr_offset += gap;

					return result;
				}

				Result append(char const *src, size_t src_len)
				{
					return _with_out_buffer([&] (Out_buffer &dst) {
						return dst.append(src, src_len); });
				}

				/**
				 * Append character, sanitize it if needed
				 */
				Result append_sanitized(char const c)
				{
					return _with_out_buffer([&] (Out_buffer &dst) {
						return dst.append_sanitized(c); });
				}

				Result append_sanitized(char const *src, size_t src_len)
				{
					return _with_out_buffer([&] (Out_buffer &dst) {
						return dst.append_sanitized(src, src_len); });
				}

				Node(Xml_generator &xml, char const *name, auto const &fn)
				:
					Node(xml, name, false, Callable<void>::Fn { fn })
				{ }

				bool has_content() const { return _has_content; }
				bool is_indented() const { return _is_indented; }
				bool exceeded()    const { return _exceeded;    }
		};

		Out_buffer _out_buffer;
		Node      *_curr_node   = 0;
		unsigned   _curr_indent = 0;
		bool       _exceeded = false;

		Xml_generator(char *dst, size_t dst_len, char const *name, auto const &fn)
		:
			_out_buffer(dst, dst_len)
		{
			if (dst) {
				node(name, fn);
				_exceeded |= _out_buffer.append('\n').exceeded
				          || _out_buffer.append('\0').exceeded;
			}
		}

	public:

		using Result   = Attempt<size_t, Buffer_error>;
		using Tag_name = String<64>;

		/**
		 * Fill buffer with XML generated by 'fn'
		 */
		static Result generate(Byte_range_ptr const &buffer,
		                       Tag_name       const &tag, auto const &fn)
		{
			Xml_generator xml(buffer.start, buffer.num_bytes, tag.string(),
				[&] { fn(xml); });

			if (xml._exceeded)
				return Buffer_error::EXCEEDED;

			return xml._out_buffer.used();
		}

		void node(char const *name, auto const &fn = [] { } )
		{
			if (!_exceeded)
				_exceeded |= Node(*this, name, fn).exceeded();
		}

		void node(char const *name) { node(name, [] { }); }

		void attribute(char const *name, char const *str, size_t str_len)
		{
			_exceeded |= _curr_node->insert_attribute(name, str, str_len).exceeded;
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
		 * Append content to XML node
		 *
		 * This method must not be followed by calls of 'attribute'.
		 */
		void append(char const *str, size_t str_len = ~0UL)
		{
			size_t const num_bytes = (str_len == ~0UL) ? strlen(str) : str_len;
			_exceeded |= _curr_node->append(str, num_bytes).exceeded;
		}

		/**
		 * Append sanitized content to XML node
		 *
		 * This method must not be followed by calls of 'attribute'.
		 */
		void append_sanitized(char const *str, size_t str_len = ~0UL)
		{
			size_t const num_bytes = (str_len == ~0UL) ? strlen(str) : str_len;
			_exceeded |= _curr_node->append_sanitized(str, num_bytes).exceeded;
		}

		template <size_t N>
		void append_sanitized(String<N> const &s)
		{
			s.with_span([&] (Span const &s) { append_sanitized(s.start, s.num_bytes); });
		}

		/**
		 * Append printable objects to XML node as sanitized content
		 *
		 * This method must not be followed by calls of 'attribute'.
		 */
		void append_content(auto &&... args)
		{
			struct Node_output : Genode::Output
			{
				Node &node;
				bool  exceeded = false;

				Node_output(Node &node) : node(node) { }

				void out_char(char c) override {
					exceeded |= node.append_sanitized(c).exceeded; }

				void out_string(char const *str, size_t n) override {
					exceeded |= node.append_sanitized(str, n).exceeded; }

			} output { *_curr_node };

			Output::out_args(output, args...);

			_exceeded |= output.exceeded;
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

			bool quoted = false;
			node.for_each_quoted_line([&] (auto const &line) {
				quoted = true;
				append_content(line);
				if (!line.last) append_sanitized("\n");
			});
			if (quoted)
				return true;

			bool ok = true;
			node.for_each_sub_node([&] (auto const &sub_node) {
				if (ok)
					ok = append_node(sub_node, { max_depth.value - 1 }); });
			return ok;
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
};

#endif /* _INCLUDE__UTIL__XML_GENERATOR_H_ */
