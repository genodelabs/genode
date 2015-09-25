/*
 * \brief  XML parser
 * \author Norman Feske
 * \date   2007-08-21
 */

/*
 * Copyright (C) 2007-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__XML_NODE_H_
#define _INCLUDE__UTIL__XML_NODE_H_

#include <util/token.h>
#include <base/exception.h>

namespace Genode {

	class Xml_attribute;
	class Xml_node;
}


/**
 * Representation of an XML-node attribute
 *
 * An attribute has the form 'name="value"'.
 */
class Genode::Xml_attribute
{
	private:

		/**
		 * Scanner policy that accepts hyphens in identifiers
		 */
		struct Scanner_policy_xml_identifier {
			static bool identifier_char(char c, unsigned i) {
				return is_letter(c) || c == '_' || c == ':'
				    || (i && (c == '-' || c == '.' || is_digit(c))); } };

		/**
		 * Define tokenizer that matches XML tags (with hyphens) as identifiers
		 */
		typedef ::Genode::Token<Scanner_policy_xml_identifier> Token;

		Token _name;
		Token _value;

		friend class Xml_node;

		/*
		 * Even though 'Tag' is part of 'Xml_node', the friendship
		 * to 'Xml_node' does not apply for 'Tag' when compiling
		 * the code with 'gcc-3.4'. Hence, we need to add an
		 * explicit friendship to 'Tag'.
		 */
		friend class Tag;

		/**
		 * Constructor
		 *
		 * This constructor is meant to be used as implicitly to
		 * construct an 'Xml_attribute' from a token sequence via an
		 * assignment from the leading 'Token'.
		 */
		Xml_attribute(Token t) :
			_name(t.eat_whitespace()), _value(_name.next().next())
		{
			if (_name.type() != Token::IDENT)
				throw Nonexistent_attribute();

			if (_name.next()[0] != '=' || _value.type() != Token::STRING)
				throw Invalid_syntax();
		}

		/**
		 * Return token following the attribute declaration
		 */
		Token _next() const { return _name.next().next().next(); }

	public:

		/*********************
		 ** Exception types **
		 *********************/

		class Invalid_syntax        : public Exception { };
		class Nonexistent_attribute : public Exception { };


		/**
		 * Return attribute type as null-terminated string
		 */
		void type(char *dst, size_t max_len) const
		{
			/*
			 * Limit number of characters by token length, take
			 * null-termination into account.
			 */
			max_len = min(max_len, _name.len() + 1);
			strncpy(dst, _name.start(), max_len);
		}

		/**
		 * Return true if attribute has specified type
		 */
		bool has_type(const char *type) {
			return strlen(type) == _name.len() &&
			       strcmp(type, _name.start(), _name.len()) == 0; }

		/**
		 * Return size of value
		 */
		size_t      value_size() const { return _value.len() - 2; }
		char const *value_base() const { return _value.start() + 1; }

		/**
		 * Return attribute value as null-terminated string
		 */
		void value(char *dst, size_t max_len) const
		{
			/*
			 * The value of 'max_len' denotes the maximum number of
			 * characters to be written to 'dst' including the null
			 * termination. From the quoted value string, we strip
			 * both quote characters and add a null-termination
			 * character.
			 */
			max_len = min(max_len, _value.len() - 2 + 1);
			strncpy(dst, _value.start() + 1, max_len);
		}

		/**
		 * Return true if attribute has the specified value
		 */
		bool has_value(const char *value) const {
			return strlen(value) == (_value.len() - 2) &&
			       !strcmp(value, _value.start() + 1, _value.len() - 2); }

		/**
		 * Return attribute value as typed value
		 *
		 * \param T  type of value to read
		 * \return   true on success, or
		 *           false if attribute is invalid or value
		 *           conversion failed
		 */
		template <typename T>
		bool value(T *out) const
		{
			/*
			 * The '_value' token starts with a quote, which we
			 * need to skip to access the string. For validating
			 * the length, we have to consider both the starting
			 * and the trailing quote character.
			 */
			return ascii_to(_value.start() + 1, *out) == _value.len() - 2;
		}

		/**
		 * Return attribute value as Genode::String
		 */
		template <size_t N>
		void value(String<N> *out) const
		{
			char buf[N];
			value(buf, sizeof(buf));
			*out = String<N>(buf);
		}

		/**
		 * Return next attribute in attribute list
		 */
		Xml_attribute next() const { return Xml_attribute(_next()); }
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

				Token _token;
				Token _name;
				Type  _type;

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
				Tag(Token start) : _token(start), _type(INVALID)
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
						try {
							for (Xml_attribute a = _name.next(); ; a = a._next())
								delimiter = a._next();
						} catch (Nonexistent_attribute) { }

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
				Tag() : _type(INVALID) { }

				/**
				 * Return type of tag
				 */
				Type type() const { return _type; }

				/**
				 * Return true if tag is the start of a valid XML node
				 */
				bool is_node() const { return _type == START || _type == EMPTY; }

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
				 * Return first attribute of tag
				 */
				Xml_attribute attribute() const { return Xml_attribute(_name.next()); }
		};

		class Comment
		{
			private:

				Token _next;   /* token following the comment */
				bool  _valid;  /* true if comment is well formed */

				/**
				 * Check if token sequence matches specified character sequence
				 *
				 * \param t  start of token sequence
				 * \param s  null-terminated character sequence
				 */
				static bool _match(Token t, const char *s)
				{
					for (int i = 0; s[i]; t = t.next(), i++)
						if (t[0] != s[i])
							return false;
					return true;
				}

			public:

				/**
				 * Constructor
				 *
				 * \param start  first token of the comment tag
				 */
				Comment(Token t) : _valid(false)
				{
					/* check for comment-start tag */
					if (!_match(t, "<!--"))
						return;

					/* search for comment-end tag */
					for ( ; t && !_match(t, "-->"); t = t.next());

					if (t.type() == Token::END)
						return;

					_next  = t.next().next().next();
					_valid = true;
				}

				/**
				 * Default constructor produces invalid Comment
				 */
				Comment() : _valid(false) { }

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

		const char *_addr;          /* first character of XML data      */
		size_t      _max_len;       /* length of XML data in characters */
		int         _num_sub_nodes; /* number of immediate sub nodes    */
		Tag         _start_tag;
		Tag         _end_tag;

		/**
		 * Search for end tag of XML node and initialize '_num_sub_nodes'
		 *
		 * \return  end tag or invalid tag
		 *
		 * The method searches for a end tag that matches the same
		 * depth level and the same name as the start tag of the XML
		 * node. If the XML structure is invalid, the search results
		 * is an invalid Tag.
		 *
		 * During the search, the method also counts the number of
		 * immediate sub nodes.
		 */
		Tag _init_end_tag()
		{
			/*
			 * If the start tag is invalid or an empty-element tag,
			 * we use the same tag as end tag.
			 */
			if (_start_tag.type() != Tag::START)
				return _start_tag;

			int   depth = 1;
			Token curr_token = _start_tag.next_token();

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
				if (depth == 1 && curr_tag.is_node())
					_num_sub_nodes++;

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
				const char *start_name = _start_tag.name().start();
				size_t      start_len  = _start_tag.name().len();
				const char *curr_name  =   curr_tag.name().start();
				size_t      curr_len   =   curr_tag.name().len();

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
		static Token eat_whitespaces_and_comments(Token t)
		{
			while (true) {

				t = t.eat_whitespace();

				/* eat comment */
				Comment comment(t);
				if (comment.valid()) {
					t = comment.next_token();
					continue;
				}

				break;
			}
			return t;
		}

		/**
		 * Create sub node from XML node
		 *
		 * \throw Nonexistent_sub_node
		 * \throw Invalid_syntax
		 */
		Xml_node _sub_node(const char *at) const
		{
			if (at < addr() || (size_t)(at - addr()) >= _max_len)
				throw Nonexistent_sub_node();

			return Xml_node(at, _max_len - (at - addr()));
		}

	public:

		/**
		 * Constructor
		 *
		 * The constructor validates if the start tag has a
		 * matching end tag of the same depth and counts
		 * the number of immediate sub nodes.
		 */
		Xml_node(const char *addr, size_t max_len = ~0UL) :
			_addr(addr),
			_max_len(max_len),
			_num_sub_nodes(0),
			_start_tag(eat_whitespaces_and_comments(Token(addr, max_len))),
			_end_tag(_init_end_tag())
		{
			/* check validity of XML node */
			if (_start_tag.type() == Tag::EMPTY) return;
			if (_start_tag.type() == Tag::START && _end_tag.type() == Tag::END) return;

			throw Invalid_syntax();
		}

		/**
		 * Request type name of XML node as null-terminated string
		 */
		void type_name(char *dst, size_t max_len) const {
			_start_tag.name().string(dst, max_len); }

		/**
		 * Return true if tag is of specified type
		 */
		bool has_type(const char *type) const {
			return (!strcmp(type, _start_tag.name().start(),
			                      _start_tag.name().len())
			      && strlen(type) == _start_tag.name().len()); }

		/**
		 * Request content of XML node as null-terminated string
		 */
		void value(char *dst, size_t max_len) const {
			max_len = min(content_size() + 1, min(max_len, _max_len));
			strncpy(dst, content_addr(), max_len); }

		/**
		 * Read content as typed value from XML node
		 *
		 * \param T    type of value to read from XML node
		 * \param out  resulting value
		 * \return     true on success
		 */
		template <typename T>
		bool value(T *out) const {
			return ascii_to(content_addr(), *out) == content_size(); }

		/**
		 * Return begin of node including the start tag
		 */
		const char *addr() const { return _addr; }

		/**
		 * Return size of node including start and end tags
		 */
		size_t size() const { return _end_tag.next_token().start() - addr(); }

		/**
		 * Return begin of node content as an opaque string
		 *
		 * Note that the returned string is not null-terminated as it
		 * points directly into a sub range of the unmodified Xml_node
		 * address range.
		 *
		 * XXX This method is deprecated. Use 'content_base()' instead.
		 *
		 * \noapi
		 */
		char *content_addr() const { return _start_tag.next_token().start(); }

		/**
		 * Return pointer to start of content
		 */
		char const *content_base() const { return content_addr(); }

		/**
		 * Return size of node content
		 */
		size_t content_size() const
		{
			if (_start_tag.type() == Tag::EMPTY)
				return 0;

			return _end_tag.token().start() - content_addr();
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
			char const *src        = content_base();
			size_t      src_len    = content_size();

			for (; dst_len > 1 && src_len; result_len++) {

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
			size_t const len = decoded_content(buf, sizeof(buf));
			buf[min(len, STRING::capacity())] = 0;
			return STRING(buf);
		}

		/**
		 * Return the number of the XML node's immediate sub nodes
		 */
		size_t num_sub_nodes() const { return _num_sub_nodes; }

		/**
		 * Return XML node following the current one
		 *
		 * \throw Nonexistent_sub_node  sub sequent node does not exist
		 */
		Xml_node next() const
		{
			Token after_node = _end_tag.next_token();
			after_node = eat_whitespaces_and_comments(after_node);
			try { return _sub_node(after_node.start()); }
			catch (Invalid_syntax) { throw Nonexistent_sub_node(); }
		}

		/**
		 * Return next XML node of specified type
		 *
		 * \param type  type of XML node, or
		 *              0 for matching any type
		 */
		Xml_node next(const char *type) const
		{
			Xml_node node = next();
			for (; type && !node.has_type(type); node = node.next());
			return node;
		}

		/**
		 * Return true if node is the last of a node sequence
		 */
		bool is_last(const char *type = 0) const
		{
			try { next(type); return false; }
			catch (Nonexistent_sub_node) { return true; }
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
			if (_num_sub_nodes > 0) {

				/* look up node at specified index */
				try {
					Xml_node curr_node = _sub_node(content_addr());
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
		 * \throw Nonexistent_sub_node  no such sub_node exists
		 */
		Xml_node sub_node(const char *type) const
		{
			if (_num_sub_nodes > 0) {

				/* search for sub node of specified type */
				try {
					Xml_node curr_node = _sub_node(content_addr());
					for ( ; true; curr_node = curr_node.next())
						if (curr_node.has_type(type))
							return curr_node;
				} catch (...) { }
			}

			throw Nonexistent_sub_node();
		}

		/**
		 * Execute functor 'fn' for each sub node of specified type
		 */
		template <typename FN>
		void for_each_sub_node(char const *type, FN const &fn)
		{
			if (_num_sub_nodes == 0)
				return;

			Xml_node node = sub_node();
			for (int i = 0; ; node = node.next()) {

				if (!type || node.has_type(type))
					fn(node);

				if (++i == _num_sub_nodes)
					break;
			}
		}

		/**
		 * Execute functor 'fn' for each sub node
		 */
		template <typename FN>
		void for_each_sub_node(FN const &fn)
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
			/* get first attribute of the node */
			Xml_attribute a = _start_tag.attribute();

			/* skip attributes until we reach the target index */
			for (; idx > 0; idx--)
				a = a._next();

			return a;
		}

		/**
		 * Return attribute of specified type
		 *
		 * \param type                   name of attribute type
		 * \throw Nonexistent_attribute  no such attribute exists
		 * \return                       XML attribute
		 */
		Xml_attribute attribute(const char *type) const
		{
			/* iterate, beginning with the first attribute of the node */
			for (Xml_attribute a = _start_tag.attribute(); ; a = a.next())
				if (a.has_type(type))
					return a;
		}

		/**
		 * Shortcut for reading an attribute value from XML node
		 *
		 * \param type           attribute name
		 * \param default_value  value returned if no attribute with the
		 *                       name 'type' is present.
		 * \return               attribute value or specified default value
		 *
		 * Without this shortcut, attribute values can be obtained by
		 * 'node.attribute(...).value(...)' only. Because the attribute
		 * lookup may throw a 'Nonexistent_attribute' exception, code that
		 * reads optional attributes (those with default values) has to
		 * handle the exception accordingly. Such code tends to become
		 * clumsy, in particular when many attributes are processed in a
		 * subsequent fashion. This method template relieves the XML node
		 * user from implementing the exception handling manually.
		 */
		template <typename T>
		inline T attribute_value(char const *type, T default_value) const
		{
			T result = default_value;
			try { attribute(type).value(&result); } catch (...) { }
			return result;
		}

		/**
		 * Return true if attribute of specified type exists
		 */
		inline bool has_attribute(char const *type) const
		{
			try { attribute(type); return true; } catch (...) { }
			return false;
		}

		/**
		 * Return true if sub node of specified type exists
		 */
		inline bool has_sub_node(char const *type) const
		{
			try { sub_node(type); return true; } catch (...) { }
			return false;
		}
};

#endif /* _INCLUDE__UTIL__XML_NODE_H_ */
