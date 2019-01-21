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

		typedef String<64> Name;
		Name name() const {
			return Name(Cstring(_name.start(), _name.len())); }

		/**
		 * Return true if attribute has specified type
		 */
		bool has_type(const char *type) {
			return strlen(type) == _name.len() &&
			       strcmp(type, _name.start(), _name.len()) == 0; }

		/**
		 * Return size of value
		 *
		 * \deprecated use 'with_raw_node' instead
		 */
		char const *value_base() const { return _value.start() + 1; }

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
			return _value.len() - 2;
		}

		/**
		 * Return true if attribute has the specified value
		 */
		bool has_value(const char *value) const {
			return strlen(value) == (_value.len() - 2)
			    && !strcmp(value, _value.start() + 1, _value.len() - 2); }

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
			fn(_value.start() + 1, value_size());
		}

		/**
		 * Return attribute value as null-terminated string
		 *
		 * \deprecated
		 */
		void value(char *dst, size_t max_len) const
		{
			with_raw_value([&] (char const *start, size_t length) {
				Genode::strncpy(dst, start, min(max_len, length + 1)); });
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
		 * Return attribute value as 'Genode::String'
		 *
		 * \deprecated  use 'value(String<N> &out' instead
		 */
		template <size_t N>
		void value(String<N> *out) const
		{
			with_raw_value([&] (char const *start, size_t length) {
				*out = String<N>(Cstring(start, length)); });
		}

		/**
		 * Return attribute value as typed value
		 *
		 * \deprecated  use 'value(T &out)' instead
		 */
		template <typename T>
		bool value(T *out) const
		{
			bool result = false;

			with_raw_value([&] (char const *start, size_t length) {
				result = (ascii_to(start, *out) == length); });

			return result;
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

		int _num_sub_nodes { 0 }; /* number of immediate sub nodes */

		const char * _addr;       /* first character of XML data */
		size_t       _max_len;    /* length of XML data in characters */
		Tag          _start_tag;
		Tag          _end_tag;

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
				const char *start_name = start_tag.name().start();
				size_t      start_len  = start_tag.name().len();
				const char *curr_name  =  curr_tag.name().start();
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

		/**
		 * Create sub node from XML node
		 *
		 * \throw Nonexistent_sub_node
		 * \throw Invalid_syntax
		 */
		Xml_node _sub_node(const char *at) const
		{
			if (at < _addr || (size_t)(at - _addr) >= _max_len)
				throw Nonexistent_sub_node();

			return Xml_node(at, _max_len - (at - _addr));
		}

		/**
		 * Return pointer to start of content
		 */
		char const *_content_base() const { return _start_tag.next_token().start(); }

	public:

		/**
		 * Constructor
		 *
		 * The constructor validates if the start tag has a matching end tag of
		 * the same depth and counts the number of immediate sub nodes.
		 *
		 * \throw Invalid_syntax
		 */
		Xml_node(const char *addr, size_t max_len = ~0UL)
		:
			_addr(addr),
			_max_len(max_len),
			_start_tag(skip_non_tag_characters(Token(addr, max_len))),
			_end_tag(_search_end_tag(_start_tag, _num_sub_nodes))
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
		 * Return size of node including start and end tags in bytes
		 */
		size_t size() const { return _end_tag.next_token().start() - _addr; }

		/**
		 * Return pointer to start of node
		 *
		 * \deprecated  use 'with_raw_node' instead
		 */
		char const *addr() const { return _addr; }

		/**
		 * Return size of node content
		 */
		size_t content_size() const
		{
			if (_start_tag.type() == Tag::EMPTY)
				return 0;

			return _end_tag.token().start() - _content_base();
		}

		/**
		 * Request type name of XML node as null-terminated string
		 */
		typedef String<64> Type;
		Type type() const
		{
			Token name = _start_tag.name();
			return Type(Cstring(name.start(), name.len()));
		}

		/**
		 * Return true if tag is of specified type
		 */
		bool has_type(const char *type) const {
			return (!strcmp(type, _start_tag.name().start(),
			                      _start_tag.name().len())
			      && strlen(type) == _start_tag.name().len()); }

		/**
		 * Call functor 'fn' with the node data '(char const *, size_t)'
		 */
		template <typename FN>
		void with_raw_node(FN const &fn) const
		{
			fn(_addr, _end_tag.next_token().start() - _addr);
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
			if (_start_tag.type() == Tag::EMPTY)
				return;

			fn(_content_base(), content_size());
		}

		/**
		 * Return pointer to start of content
		 *
		 * XXX This method is deprecated. Use 'with_raw_content()' instead.
		 *
		 * \noapi
		 */
		char *content_addr() const { return _start_tag.next_token().start(); }

		/**
		 * Return pointer to start of content
		 *
		 * XXX This method is deprecated. Use 'with_raw_content()' instead.
		 *
		 * \noapi
		 */
		char const *content_base() const { return content_addr(); }

		/**
		 * Return content as out value
		 *
		 * \deprecated  use with_raw_content instead
		 */
		template <typename T> bool value(T *out) const { return value(*out); }

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
		size_t num_sub_nodes() const { return _num_sub_nodes; }

		/**
		 * Return XML node following the current one
		 *
		 * \throw Nonexistent_sub_node  sub sequent node does not exist
		 */
		Xml_node next() const
		{
			Token after_node = _end_tag.next_token();
			after_node = skip_non_tag_characters(after_node);
			try { return _sub_node(after_node.start()); }
			catch (Invalid_syntax) { throw Nonexistent_sub_node(); }
		}

		/**
		 * Return next XML node of specified type
		 *
		 * \param type  type of XML node, or nullptr for matching any type
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
		bool last(const char *type = 0) const
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
					Xml_node curr_node = _sub_node(_content_base());
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
					Xml_node curr_node = _sub_node(_content_base());
					for ( ; true; curr_node = curr_node.next())
						if (curr_node.has_type(type))
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
		void with_sub_node(char const *type, FN const &fn) const
		{
			if (has_sub_node(type))
				fn(sub_node(type));
		}

		/**
		 * Execute functor 'fn' for each sub node of specified type
		 */
		template <typename FN>
		void for_each_sub_node(char const *type, FN const &fn) const
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
		inline T attribute_value(char const *type, T const default_value) const
		{
			T result = default_value;
			try { attribute(type).value(result); } catch (...) { }
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

#endif /* _INCLUDE__UTIL__XML_NODE_H_ */
