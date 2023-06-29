/*
 * \brief  XML parser
 * \author Norman Feske
 * \date   2007-08-21
 */

/*
 * Copyright (C) 2007-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__XML_NODE_H_
#define _INCLUDE__UTIL__XML_NODE_H_

#include <base/log.h>
#include <util/token.h>
#include <util/noncopyable.h>
#include <base/exception.h>

namespace Genode {
	class Xml_attribute;
	class Xml_node;
	class Xml_unquoted;
}


/**
 * Representation of an XML-node attribute
 *
 * An attribute has the form 'name="value"'.
 */
class Genode::Xml_attribute
{
	private:

		struct Scanner_policy_xml_identifier
		{
			static bool identifier_char(char c, unsigned i)
			{
				/* accepts hyphens in identifiers */
				return is_letter(c) || c == '_' || c == ':'
				    || (i && (c == '-' || c == '.' || is_digit(c)));
			}

			static bool end_of_quote(const char *s) { return s[1] == '\"'; }
		};

		/**
		 * Define tokenizer that matches XML tags (with hyphens) as identifiers
		 */
		typedef ::Genode::Token<Scanner_policy_xml_identifier> Token;

		struct Tokens
		{
			Token name;
			Token equals { name  .next().eat_whitespace() };
			Token value  { equals.next().eat_whitespace() };

			Tokens(Token t) : name(t.eat_whitespace()) { };

			bool valid() const
			{
				return (name.type()  == Token::IDENT)
				    && (equals[0]    == '=')
				    && (value.type() == Token::STRING);
			}
		} _tokens;

		friend class Xml_node;

		/*
		 * Even though 'Tag' is part of 'Xml_node', the friendship
		 * to 'Xml_node' does not apply for 'Tag' when compiling
		 * the code with 'gcc-3.4'. Hence, we need to add an
		 * explicit friendship to 'Tag'.
		 */
		friend class Tag;

		/**
		 * Return true if token refers to a valid attribute
		 */
		static bool _valid(Token t) { return Tokens(t).valid(); }

		/**
		 * Constructor
		 *
		 * This constructor is meant to be used as implicitly to
		 * construct an 'Xml_attribute' from a token sequence via an
		 * assignment from the leading 'Token'.
		 */
		explicit Xml_attribute(Token t) : _tokens(t)
		{
			if (_tokens.name.type() != Token::IDENT)
				throw Nonexistent_attribute();

			if (!_tokens.valid())
				throw Invalid_syntax();
		}

		/**
		 * Return token following the attribute declaration
		 */
		Token _next_token() const { return _tokens.value.next(); }

	public:

		/*********************
		 ** Exception types **
		 *********************/

		class Invalid_syntax        : public Exception { };
		class Nonexistent_attribute : public Exception { };


		typedef String<64> Name;
		Name name() const {
			return Name(Cstring(_tokens.name.start(), _tokens.name.len())); }

		/**
		 * Return true if attribute has specified type
		 */
		bool has_type(char const *type) {
			return strlen(type) == _tokens.name.len() &&
			       strcmp(type, _tokens.name.start(), _tokens.name.len()) == 0; }

		/**
		 * Return size of the value in bytes
		 */
		size_t value_size() const
		{
			/*
			 * The size of the actual value content excludes both the starting
			 * and the trailing quote character.
			 *
			 * The invariant 'len >= 2' is enforced by the 'Xml_attribute'
			 * constructor by checking the '_value' type for being a 'STRING'.
			 */
			return _tokens.value.len() - 2;
		}

		/**
		 * Return true if attribute has the specified value
		 */
		bool has_value(char const *value) const {
			return strlen(value) == (_tokens.value.len() - 2)
			    && !strcmp(value, _tokens.value.start() + 1, _tokens.value.len() - 2); }

		/**
		 * Call functor 'fn' with the data of the attribute value as argument
		 *
		 * The functor is called with the start pointer ('char const *') and
		 * size (size_t) of the attribute value as arguments.
		 *
		 * Note that the content of the buffer is not null-terminated but
		 * delimited by the size argument.
		 */
		template <typename FN>
		void with_raw_value(FN const &fn) const
		{
			/*
			 * Skip leading quote of the '_value' to access the actual value.
			 */
			fn(_tokens.value.start() + 1, value_size());
		}

		/**
		 * Return attribute value as typed value
		 *
		 * \param T  type of value to read
		 * \return   true on success, or
		 *           false if attribute is invalid or value
		 *           conversion failed
		 */
		template <typename T>
		bool value(T &out) const
		{
			bool result = false;

			with_raw_value([&] (char const *start, size_t length) {
				result = (ascii_to(start, out) == length); });

			return result;
		}

		/**
		 * Return attribute value as 'Genode::String'
		 */
		template <size_t N>
		void value(String<N> &out) const
		{
			with_raw_value([&] (char const *start, size_t length) {
				out = String<N>(Cstring(start, length)); });
		}

		/**
		 * Return next attribute in attribute list
		 */
		Xml_attribute next() const { return Xml_attribute(_next_token()); }
};


/**
 * Representation of an XML node
 */
class Genode::Xml_node
{
	private:

		typedef Xml_attribute::Token Token;

		/**
		 * Forward declaration needed for befriending Tag with Xml_attribute
		 */
		class Tag;

		friend class Xml_unquoted;

	public:

		/*********************
		 ** Exception types **
		 *********************/

		typedef Genode::Exception                    Exception;
		typedef Xml_attribute::Nonexistent_attribute Nonexistent_attribute;
		typedef Xml_attribute::Invalid_syntax        Invalid_syntax;

		class Nonexistent_sub_node  : public Exception { };


		/**
		 * Type definition for maintaining backward compatibility
		 */
		typedef Xml_attribute Attribute;

	private:

		class Tag
		{
			public:

				enum Type { START, END, EMPTY, INVALID };

			private:

				Token _token { };
				Token _name  { };
				Type  _type  { INVALID };

			public:

				/**
				 * Constructor
				 *
				 * \param start  first token of the tag
				 *
				 * At construction time, the validity of the tag is checked and
				 * the tag type is determined. A valid tag consists of:
				 * # Leading '<' tag delimiter
				 * # '/' for marking an end tag
				 * # Tag name
				 * # Optional attribute sequence (if tag is no end tag)
				 * # '/' for marking an empty-element tag (if tag is no end tag)
				 * # Closing '>' tag delimiter
				 */
				Tag(Token start) : _token(start)
				{
					Type supposed_type = START;

					if (_token[0] != '<')
						return;

					if (_token.next()[0] == '/')
						supposed_type = END;

					if (_token.next().type() != Token::IDENT && _token.next()[0] != '/')
						return;

					_name = _token.next()[0] == '/' ? _token.next().next() : _token.next();
					if (_name.type() != Token::IDENT)
						return;

					/* skip attributes to find tag delimiter */
					Token delimiter = _name.next();
					if (supposed_type != END)
						while (Xml_attribute::_valid(delimiter))
							delimiter = Xml_attribute(delimiter)._next_token();

					delimiter = delimiter.eat_whitespace();

					/*
					 * Now we expect the '>' delimiter. For empty-element tags,
					 * the delimiter is prefixed with a '/'.
					 */
					if (delimiter[0] == '/') {

						/* if a '/' was already at the start, the tag is invalid */
						if (supposed_type == END)
							return;

						supposed_type = EMPTY;

						/* skip '/' */
						delimiter = delimiter.next();
					}

					if (delimiter[0] != '>') return;

					_type  = supposed_type;
				}

				/**
				 * Default constructor produces invalid Tag
				 */
				Tag() { }

				/**
				 * Return type of tag
				 */
				Type type() const { return _type; }

				/**
				 * Return true if tag is the start of a valid XML node
				 */
				bool node() const { return _type == START || _type == EMPTY; }

				/**
				 * Return first token of tag
				 */
				Token token() const { return _token; }

				/**
				 * Return name of tag
				 */
				Token name() const { return _name; }

				/**
				 * Return token after the closing tag delimiter
				 */
				Token next_token() const
				{
					/*
					 * Search for next closing delimiter, skip potential
					 * attributes and '/' delimiter prefix of empty-element
					 * tags.
					 */
					Token t = _name;
					for (; t && t[0] != '>'; t = t.next());

					/* if 't' is invalid, 't.next()' is invalid too */
					return t.next();
				}

				/**
				 * Return true if tag has at least one attribute
				 */
				bool has_attribute() const { return Xml_attribute::_valid(_name.next()); }

				/**
				 * Return first attribute of tag
				 */
				Xml_attribute attribute() const { return Xml_attribute(_name.next()); }
		};

		class Comment
		{
			private:

				Token _next  { };        /* token following the comment */
				bool  _valid { false };  /* true if comment is well formed */

			public:

				/**
				 * Constructor
				 *
				 * \param start  first token of the comment tag
				 */
				Comment(Token t)
				{
					/* check for comment start */
					if (!t.matches("<!--"))
						return;

					/* skip four single characters for "<!--" */
					t = t.next().next().next().next();

					/* find token after comment delimiter */
					_next  = t.next_after("-->");
					_valid = _next.valid();
				}

				/**
				 * Default constructor produces invalid Comment
				 */
				Comment() { }

				/**
				 * Return true if comment is valid
				 */
				bool valid() const { return _valid; }

				/**
				 * Return token after the closing comment delimiter
				 */
				Token next_token() const { return _next; }
		};

		/**
		 * Helper class to decode XML character entities
		 */
		struct Decoded_character
		{
			char   character   = 0;
			size_t encoded_len = 1;

			struct Translation
			{
				char        character;
				char const *seq;
				size_t      seq_len;
			};

			static Translation translate(char const *src, size_t src_len)
			{
				enum { NUM = 6 };
				static Translation translations[NUM] = {
					{ '>',  "&gt;",   4 },
					{ '<',  "&lt;",   4 },
					{ '&',  "&amp;",  5 },
					{ '"',  "&quot;", 6 },
					{ '\'', "&apos;", 6 },
					{ 0,    "&#x00;", 6 }
				};

				if (src_len == 0)
					return { 0, nullptr, 0 };

				for (unsigned i = 0; i < NUM; i++) {

					Translation const &translation = translations[i];

					if (src_len < translation.seq_len
					 || memcmp(src, translation.seq, translation.seq_len))
						continue;

					/* translation matches */
					return translation;
				}

				/* sequence is not known, pass single character as is */
				return { *src, nullptr, 1 };
			}

			Decoded_character(char const *src, size_t src_len)
			{
				if (*src != '&' || src_len == 0) {
					character = *src;
					return;
				}

				Translation const translation = translate(src, src_len);

				character   = translation.character;
				encoded_len = translation.seq_len;
			}
		};

		char const * _addr;       /* first character of XML data */
		size_t       _max_len;    /* length of XML data in characters */

		/**
		 * Search matching end tag for given start tag and detemine number of
		 * immediate sub nodes along the way.
		 *
		 * \return  end tag or invalid tag
		 *
		 * The method searches for a end tag that matches the same depth level
		 * and the same name as the start tag of the XML node. If the XML
		 * structure is invalid, the search results is an invalid Tag.
		 */
		static Tag _search_end_tag(Tag start_tag, int &sub_nodes_count)
		{
			/*
			 * If the start tag is invalid or an empty-element tag,
			 * we use the same tag as end tag.
			 */
			if (start_tag.type() != Tag::START)
				return start_tag;

			int   depth = 1;
			Token curr_token = start_tag.next_token();

			while (curr_token.type() != Token::END) {

				/* eat XML comment */
				Comment curr_comment(curr_token);
				if (curr_comment.valid()) {
					curr_token = curr_comment.next_token();
					continue;
				}

				/* skip all tokens that are no tags */
				Tag curr_tag(curr_token);
				if (curr_tag.type() == Tag::INVALID) {
					curr_token = curr_token.next();
					continue;
				}

				/* count sub nodes at depth 1 */
				if (depth == 1 && curr_tag.node())
					sub_nodes_count++;

				/* keep track of the current depth */
				depth += (curr_tag.type() == Tag::START);
				depth -= (curr_tag.type() == Tag::END);

				/* within sub nodes, continue after current token */
				if (depth > 0) {

					/* continue search with token after current tag */
					curr_token = curr_tag.next_token();
					continue;
				}

				/* reaching the same depth as the start tag */
				char const *start_name = start_tag.name().start();
				size_t      start_len  = start_tag.name().len();
				char const *curr_name  =  curr_tag.name().start();
				size_t      curr_len   =  curr_tag.name().len();

				/* on mismatch of start tag and end tag, return invalid tag */
				if (start_len != curr_len
				 || strcmp(start_name, curr_name, curr_len))
					return Tag();

				/* end tag corresponds to start tag */
				return curr_tag;
			}
			return Tag();
		}

		/**
		 * Find next non-whitespace and non-comment token
		 */
		static Token skip_non_tag_characters(Token t)
		{
			while (true) {

				t = t.eat_whitespace();

				/* eat comment */
				Comment comment(t);
				if (comment.valid()) {
					t = comment.next_token();
					continue;
				}

				/* skip token if it is valid but does not start a tag */
				Tag curr_tag(t);
				if (curr_tag.type() == Tag::INVALID && curr_tag.token()) {
					t = t.next();
					continue;
				}

				break;
			}
			return t;
		}

		struct Tags
		{
			int num_sub_nodes = 0;
			Tag start;
			Tag end;

			Tags(char const *addr, size_t max_len)
			:
				start(skip_non_tag_characters(Token(addr, max_len))),
				end(_search_end_tag(start, num_sub_nodes))
			{ }
		} _tags;

		/**
		 * Return true if specified buffer contains a valid XML node
		 */
		static bool _valid(Tags const &tags)
		{
			if (tags.start.type() == Tag::EMPTY)
				return true;

			if (tags.start.type() == Tag::START && tags.end.type() == Tag::END)
				return true;

			return false;
		}

		bool _valid_node_at(char const *at) const
		{
			bool const in_range = (at >= _addr && (size_t)(at - _addr) < _max_len);

			return in_range && _valid(Tags(at, _max_len - (at - _addr)));
		}

		/**
		 * Create sub node from XML node
		 *
		 * \throw Nonexistent_sub_node
		 * \throw Invalid_syntax
		 */
		Xml_node _node_at(char const *at) const
		{
			if (!_valid_node_at(at))
				throw Nonexistent_sub_node();

			return Xml_node(at, _max_len - (at - _addr));
		}

		/**
		 * Return pointer to start of content
		 */
		char const *_content_base() const { return _tags.start.next_token().start(); }

	public:

		/**
		 * Constructor
		 *
		 * The constructor validates if the start tag has a matching end tag of
		 * the same depth and counts the number of immediate sub nodes.
		 *
		 * \throw Invalid_syntax
		 */
		Xml_node(char const *addr, size_t max_len = ~0UL)
		:
			_addr(addr), _max_len(max_len), _tags(addr, max_len)
		{
			if (!_valid(_tags))
				throw Invalid_syntax();
		}

		/**
		 * Return size of node including start and end tags in bytes
		 */
		size_t size() const { return _tags.end.next_token().start() - _addr; }

		/**
		 * Return size of node content
		 */
		size_t content_size() const
		{
			if (_tags.start.type() == Tag::EMPTY)
				return 0;

			return _tags.end.token().start() - _content_base();
		}

		/**
		 * Request type name of XML node as null-terminated string
		 */
		typedef String<64> Type;
		Type type() const
		{
			Token name = _tags.start.name();
			return Type(Cstring(name.start(), name.len()));
		}

		/**
		 * Return true if tag is of specified type
		 */
		bool has_type(char const *type) const {
			return (!strcmp(type, _tags.start.name().start(),
			                      _tags.start.name().len())
			      && strlen(type) == _tags.start.name().len()); }

		/**
		 * Call functor 'fn' with the node data '(char const *, size_t)'
		 */
		template <typename FN>
		void with_raw_node(FN const &fn) const
		{
			char const *start_ptr = _tags.start.token().start();
			fn(start_ptr, _tags.end.next_token().start() - start_ptr);
		}

		/**
		 * Call functor 'fn' with content '(char const *, size_t) as argument'
		 *
		 * Note that the content is not null-terminated. It points directly
		 * into a sub range of the unmodified 'Xml_node' data.
		 *
		 * If the node has no content, the functor 'fn' is not called.
		 */
		template <typename FN>
		void with_raw_content(FN const &fn) const
		{
			if (_tags.start.type() == Tag::EMPTY)
				return;

			fn(_content_base(), content_size());
		}

		/**
		 * Export decoded node content from XML node
		 *
		 * \param dst      destination buffer
		 * \param dst_len  size of destination buffer in bytes
		 * \return         number of bytes written to the destination buffer
		 *
		 * This function transforms XML character entities into their
		 * respective characters.
		 */
		size_t decoded_content(char *dst, size_t dst_len) const
		{
			size_t      result_len = 0;
			char const *src        = _content_base();
			size_t      src_len    =  content_size();

			for (; dst_len && src_len; dst_len--, result_len++) {

				Decoded_character const decoded_character(src, src_len);

				*dst++ = decoded_character.character;

				src     += decoded_character.encoded_len;
				src_len -= decoded_character.encoded_len;
			}

			return result_len;
		}

		/**
		 * Read decoded node content as Genode::String
		 */
		template <typename STRING>
		STRING decoded_content() const
		{
			char buf[STRING::capacity() + 1];
			size_t const len = decoded_content(buf, sizeof(buf) - 1);
			buf[min(len, sizeof(buf) - 1)] = 0;
			return STRING(Cstring(buf));
		}

		/**
		 * Return the number of the XML node's immediate sub nodes
		 */
		size_t num_sub_nodes() const { return _tags.num_sub_nodes; }

		/**
		 * Return XML node following the current one
		 *
		 * \throw Nonexistent_sub_node  subsequent node does not exist
		 */
		Xml_node next() const
		{
			Token after_node = _tags.end.next_token();
			after_node = skip_non_tag_characters(after_node);
			try {
				return _node_at(after_node.start());
			}
			catch (Invalid_syntax) { throw Nonexistent_sub_node(); }
		}

		/**
		 * Return next XML node of specified type
		 *
		 * \param type  type of XML node, or nullptr for matching any type
		 *
		 * \throw Nonexistent_sub_node  subsequent node does not exist
		 */
		Xml_node next(char const *type) const
		{
			Xml_node node = next();
			for (; type && !node.has_type(type); node = node.next());
			return node;
		}

		/**
		 * Return true if node is the last of a node sequence
		 */
		bool last(char const *type = nullptr) const
		{
			Token after = _tags.end.next_token();
			after = skip_non_tag_characters(after);

			for (;;) {
				if (!_valid_node_at(after.start()))
					return true;

				Xml_node node = _node_at(after.start());
				if (!type || node.has_type(type))
					return false;

				after = node._tags.end.next_token();
				after = skip_non_tag_characters(after);
			}
		}

		/**
		 * Return sub node with specified index
		 *
		 * \param  idx                   index of sub node,
		 *                               default is the first node
		 * \throw  Nonexistent_sub_node  no such sub node exists
		 */
		Xml_node sub_node(unsigned idx = 0U) const
		{
			if (_tags.num_sub_nodes > 0) {
				try {
					Xml_node curr_node = _node_at(_content_base());
					for (; idx > 0; idx--)
						curr_node = curr_node.next();
					return curr_node;
				} catch (Invalid_syntax) { }
			}
			throw Nonexistent_sub_node();
		}

		/**
		 * Return first sub node that matches the specified type
		 *
		 * \throw Nonexistent_sub_node  no such sub node exists
		 */
		Xml_node sub_node(char const *type) const
		{
			if (_tags.num_sub_nodes > 0) {

				/* search for sub node of specified type */
				try {
					Xml_node curr_node = _node_at(_content_base());
					for ( ; true; curr_node = curr_node.next())
						if (!type || curr_node.has_type(type))
							return curr_node;
				} catch (...) { }
			}
			throw Nonexistent_sub_node();
		}

		/**
		 * Apply functor 'fn' to first sub node of specified type
		 *
		 * The functor is called with the sub node as argument.
		 * If no matching sub node exists, the functor is not called.
		 */
		template <typename FN>
		void with_optional_sub_node(char const *type, FN const &fn) const
		{
			if (has_sub_node(type))
				fn(sub_node(type));
		}

		/**
		 * Apply functor 'fn' to first sub node of specified type
		 *
		 * The functor is called with the sub node as argument.
		 * If no matching sub node exists, the functor 'fn_nexists' is called.
		 */
		template <typename FN, typename FN_NEXISTS>
		void with_sub_node(char const *type, FN const &fn, FN_NEXISTS const &fn_nexists) const
		{
			if (has_sub_node(type))
				fn(sub_node(type));
			else
				fn_nexists();
		}

		/**
		 * Execute functor 'fn' for each sub node of specified type
		 */
		template <typename FN>
		void for_each_sub_node(char const *type, FN const &fn) const
		{
			if (!has_sub_node(type))
				return;

			for (Xml_node node = sub_node(type); ; node = node.next()) {
				if (!type || node.has_type(type))
					fn(node);

				if (node.last())
					break;
			}
		}

		/**
		 * Execute functor 'fn' for each sub node
		 */
		template <typename FN>
		void for_each_sub_node(FN const &fn) const
		{
			for_each_sub_node(nullptr, fn);
		}

		/**
		 * Return Nth attribute of XML node
		 *
		 * \param idx                    attribute index,
		 *                               first attribute has index 0
		 * \throw Nonexistent_attribute  no such attribute exists
		 * \return                       XML attribute
		 */
		Xml_attribute attribute(unsigned idx) const
		{
			Xml_attribute attr = _tags.start.attribute();
			for (unsigned i = 0; i < idx; i++)
				attr = Xml_attribute(attr._next_token());

			return attr;
		}

		/**
		 * Return attribute of specified type
		 *
		 * \param type                   name of attribute type
		 * \throw Nonexistent_attribute  no such attribute exists
		 * \return                       XML attribute
		 */
		Xml_attribute attribute(char const *type) const
		{
			for (Xml_attribute attr = _tags.start.attribute(); ;) {
				if (attr.has_type(type))
					return attr;

				attr = Xml_attribute(attr._next_token());
			}
			throw Nonexistent_attribute();
		}

		/**
		 * Read attribute value from XML node
		 *
		 * \param type           attribute name
		 * \param default_value  value returned if no attribute with the
		 *                       name 'type' is present.
		 * \return               attribute value or specified default value
		 *
		 * The type of the return value corresponds to the type of the
		 * default value.
		 */
		template <typename T>
		T attribute_value(char const *type, T const default_value) const
		{
			T result = default_value;

			if (!_tags.start.has_attribute())
				return result;

			for (Xml_attribute attr = _tags.start.attribute(); ; ) {

				/* match */
				if (attr.has_type(type)) {
					attr.value(result);
					return result;
				}

				/* end of attribute */
				Token const next = attr._next_token();
				if (!Xml_attribute::_valid(next))
					break;

				/* keep searching */
				attr = Xml_attribute(next);
			}

			return result;
		}

		/**
		 * Return true if attribute of specified type exists
		 */
		bool has_attribute(char const *type) const
		{
			if (!_tags.start.has_attribute())
				return false;

			if (type == nullptr)
				return true;

			for (Xml_attribute attr = _tags.start.attribute(); ; ) {
				if (attr.has_type(type))
					return true;

				Token const next = attr._next_token();
				if (!Xml_attribute::_valid(next))
					return false;

				attr = Xml_attribute(next);
			}
		}

		/**
		 * Execute functor 'fn' for each attribute
		 */
		template <typename FN>
		void for_each_attribute(FN const &fn) const
		{
			if (!_tags.start.has_attribute())
				return;

			for (Xml_attribute attr = _tags.start.attribute(); ; ) {
				fn(attr);

				Token const next = attr._next_token();
				if (!Xml_attribute::_valid(next))
					return;

				attr = Xml_attribute(next);
			}
		}

		/**
		 * Return true if sub node of specified type exists
		 */
		bool has_sub_node(char const *type) const
		{
			if (_tags.num_sub_nodes == 0)
				return false;

			if (!_valid_node_at(_content_base()))
				return false;

			if (type == nullptr)
				return true;

			/* search for node of given type */
			for (Xml_node node = _node_at(_content_base()); ; node = node.next()) {
				if (node.has_type(type))
					return true;

				if (node.last())
					break;
			}
			return false;
		}

		void print(Output &output) const {
			output.out_string(_addr, size()); }

		/**
		 * Return true if this node differs from 'another'
		 */
		bool differs_from(Xml_node const &another) const
		{
			return size() != another.size() ||
			       memcmp(_addr, another._addr, size()) != 0;
		}
};


/**
 * Utility for unquoting XML attributes
 *
 * The 'Xml_unquoted' utility can be used to revert quoted XML attribute
 * values. Such quoting is needed whenever an attribute value can contain '"'
 * characters.
 */
class Genode::Xml_unquoted : Noncopyable
{
	private:

		struct
		{
			char const *base;
			size_t      len;
		} const _content_ptr;

	public:

		template <size_t N>
		Xml_unquoted(String<N> const &string)
		: _content_ptr({ string.string(), string.length() ? string.length() - 1 : 0 })
		{ }

		void print(Output &out) const
		{
			char const *src = _content_ptr.base;
			size_t      len = _content_ptr.len;

			while (len > 0) {
				Xml_node::Decoded_character const decoded_character(src, len);

				Genode::print(out, Char(decoded_character.character));

				src += decoded_character.encoded_len;
				len -= decoded_character.encoded_len;
			}
		}
};

#endif /* _INCLUDE__UTIL__XML_NODE_H_ */
