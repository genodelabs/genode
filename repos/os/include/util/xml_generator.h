/*
 * \brief  Utility for generating XML
 * \author Norman Feske
 * \date   2014-01-07
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__XML_GENERATOR_H_
#define _INCLUDE__UTIL__XML_GENERATOR_H_

#include <util/string.h>
#include <util/print_lines.h>
#include <base/snprintf.h>

namespace Genode { class Xml_generator; }


class Genode::Xml_generator
{
	public:

		/**
		 * Exception type
		 */
		class Buffer_exceeded { };

	private:

		/**
		 * Buffer descriptor where the XML output goes to
		 *
		 * All 'append' methods may throw a 'Buffer_exceeded' exception.
		 */
		class Out_buffer
		{
			private:

				char  *_dst;
				size_t _capacity;
				size_t _used = 0;

				void _check_advance(size_t const len) const {
					if (_used + len > _capacity)
						throw Buffer_exceeded(); }

			public:

				Out_buffer(char *dst, size_t capacity)
				: _dst(dst), _capacity(capacity) { }

				void advance(size_t const len)
				{
					_check_advance(len);
					_used += len;
				}

				/**
				 * Append character
				 */
				void append(char const c)
				{
					_check_advance(1);
					_dst[_used] = c;
					advance(1);
				}

				/**
				 * Append character 'n' times
				 */
				void append(char const c, size_t n) {
					for (; n--; append(c)); }

				/**
				 * Append character buffer
				 */
				void append(char const *src, size_t len) {
					for (; len--; append(*src++)); }

				/**
				 * Append null-terminated string
				 */
				void append(char const *src) { append(src, strlen(src)); }

				/**
				 * Return unused part of the buffer
				 */
				Out_buffer remainder() const {
					return Out_buffer(_dst + _used, _capacity - _used); }

				/**
				 * Insert gap into already populated part of the buffer
				 *
				 * \return  empty buffer spanning the gap
				 */
				Out_buffer insert_gap(unsigned const at, size_t const len)
				{
					/* don't allow the insertion into non-populated part */
					if (at > _used)
						return Out_buffer(_dst + at, 0);

					_check_advance(len);
					memmove(_dst + at + len, _dst + at, _used - at);
					advance(len);

					return Out_buffer(_dst + at, len);
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
			private:

				/**
				 * Indentation level of node
				 */
				unsigned const _indent_level;

				Node * const _parent_node = 0;

				Out_buffer _out_buffer;

				bool _has_content    = false;
				bool _is_indented    = false;
				bool _has_attributes = false;

				/**
				 * Cursor position of next attribute to insert
				 */
				unsigned _attr_offset = 0;

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
						_out_buffer.append(">");

					if (indented)
						_out_buffer.append("\n");

					_has_content = true;
					_is_indented = indented;

					return _out_buffer.remainder();
				}

				/**
				 * Called by sub node
				 */
				void _commit_content(Out_buffer content_buffer)
				{
					_out_buffer.advance(content_buffer.used());
				}

			public:

				void insert_attribute(char const *name, char const *value)
				{
					/* ' ' + name + '=' + '"' + value + '"' */
					size_t const gap = 1 + strlen(name) + 1 + 1 + strlen(value) + 1;

					Out_buffer dst = _out_buffer.insert_gap(_attr_offset, gap);
					dst.append(' ');
					dst.append(name);
					dst.append("=\"");
					dst.append(value);
					dst.append("\"");

					_attr_offset += gap;
				}

				void append(char const *src, size_t src_len)
				{
					Out_buffer content_buffer = _content_buffer(false);

					content_buffer.append(src, src_len);

					_commit_content(content_buffer);
				}

				template <typename FUNC>
				Node(Xml_generator &xml, char const *name, FUNC const &func)
				:
					_indent_level(xml._curr_indent),
					_parent_node(xml._curr_node),
					_out_buffer(_parent_node ? _parent_node->_content_buffer(true)
					                         : xml._out_buffer)
				{
					_out_buffer.append('\t', _indent_level);
					_out_buffer.append("<");
					_out_buffer.append(name);
					_attr_offset = _out_buffer.used();

					xml._curr_node = this;
					xml._curr_indent++;

					/*
					 * Process attributes and sub nodes
					 */
					func();

					xml._curr_node = _parent_node;
					xml._curr_indent--;

					if (_is_indented) {
						_out_buffer.append("\n");
						_out_buffer.append('\t', _indent_level);
					}

					if (_has_content) {
						_out_buffer.append("</");
						_out_buffer.append(name);
						_out_buffer.append(">");
					} else
						_out_buffer.append("/>");

					if (_parent_node)
						_parent_node->_commit_content(_out_buffer);
					else
						xml._out_buffer = _out_buffer;

					_out_buffer.append('\0');
				}
		};

		Out_buffer _out_buffer;
		Node      *_curr_node   = 0;
		unsigned   _curr_indent = 0;

	public:

		template <typename FUNC>
		Xml_generator(char *dst, size_t dst_len,
		              char const *name, FUNC const &func)
		:
			_out_buffer(dst, dst_len)
		{
			if (dst) {
				node(name, func);
				_out_buffer.append('\n');
			}
		}

		template <typename FUNC>
		void node(char const *name, FUNC const &func = [] () { } )
		{
			Node(*this, name, func);
		}

		void node(char const *name) { Node(*this, name, [] () { }); }

		void attribute(char const *name, char const *str)
		{
			_curr_node->insert_attribute(name, str);
		}

		void attribute(char const *name, long value)
		{
			char buf[64];
			Genode::snprintf(buf, sizeof(buf), "%ld", value);
			_curr_node->insert_attribute(name, buf);
		}

		/**
		 * Append content to XML node
		 *
		 * This method must not be followed by calls of 'attribute'.
		 */
		void append(char const *str, size_t str_len = ~0UL)
		{
			_curr_node->append(str, str_len == ~0UL ? strlen(str) : str_len);
		}

		size_t used() const { return _out_buffer.used(); }
};

#endif /* _INCLUDE__UTIL__XML_GENERATOR_H_ */
