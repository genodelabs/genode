/*
 * \brief  Sub-scope types
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__SUB_SCOPES_H_
#define _INCLUDE__DIALOG__SUB_SCOPES_H_

#include <dialog/types.h>

namespace Dialog {

	template <typename AT, typename FN>
	static void with_narrowed_xml(AT const &, char const *xml_type, FN const &fn);

	struct Vbox;
	struct Hbox;
	struct Frame;
	struct Float;
	struct Button;
	struct Label;
	struct Min_ex;
	struct Depgraph;
}


template <typename AT, typename FN>
static inline void Dialog::with_narrowed_xml(AT const &at, char const *xml_type, FN const &fn)
{
	at._location.with_optional_sub_node(xml_type, [&] (Xml_node const &node) {
		AT const narrowed_at { at.seq_number, node };
		fn(narrowed_at);
	});
}


struct Dialog::Vbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("vbox", [&] { fn(s); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "vbox", fn); }
};


struct Dialog::Hbox : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("hbox", [&] { fn(s); });
	}

	template <typename SCOPE>
	static void view_sub_scope(SCOPE &s) { s.node("hbox", [&] { }); }

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "hbox", fn); }
};


struct Dialog::Frame : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("frame", [&] { fn(s); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "frame", fn); }
};


struct Dialog::Float : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("float", [&] { fn(s); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "float", fn); }
};


struct Dialog::Button : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("button", [&] {
			fn(s); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "button", fn); }
};


struct Dialog::Label : Sub_scope
{
	template <typename SCOPE, typename TEXT>
	static void view_sub_scope(SCOPE &s, TEXT const &text)
	{
		s.node("label", [&] {
			s.attribute("text", text); });
	}

	template <typename SCOPE, typename TEXT, typename FN>
	static void view_sub_scope(SCOPE &s, TEXT const &text, FN const &fn)
	{
		s.node("label", [&] {
			s.attribute("text", text);
			fn(s);
		});
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "label", fn); }
};


struct Dialog::Min_ex : Sub_scope
{
	template <typename SCOPE>
	static void view_sub_scope(SCOPE &s, unsigned min_ex)
	{
		s.node("label", [&] {
			s.attribute("min_ex", min_ex); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "label", fn); }
};


struct Dialog::Depgraph : Sub_scope
{
	template <typename SCOPE, typename FN>
	static void view_sub_scope(SCOPE &s, FN const &fn)
	{
		s.node("depgraph", [&] { fn(s); });
	}

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		with_narrowed_xml(at, "depgraph", fn); }
};

#endif /* _INCLUDE__DIALOG__SUB_SCOPES_H_ */
