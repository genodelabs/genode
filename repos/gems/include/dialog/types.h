/*
 * \brief  Fundamental types for implementing GUI dialogs
 * \author Norman Feske
 * \date   2023-03-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DIALOG__TYPES_H_
#define _INCLUDE__DIALOG__TYPES_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/xml_generator.h>
#include <base/log.h>
#include <input/event.h>
#include <input/keycodes.h>

namespace Dialog {

	using namespace Genode;

	struct Id;
	struct Event;
	struct At;
	struct Clicked_at;
	struct Clacked_at;
	struct Dragged_at;
	struct Hovered_at;
	static inline Clicked_at const &clicked_at(At const &at);
	template <typename...> struct Scope;
	struct Sub_scope;
	struct Top_level_dialog;
	template <typename>    struct Widget;
	template <typename>    struct Widget_interface;
	template <typename...> struct Hosted;
}


namespace Dialog { namespace Meta {

	template <typename, typename... TAIL>
	struct Last { using Type = typename Last<TAIL...>::Type; };

	template <typename T>
	struct Last<T> { using Type = T; };

	template <typename... ARGS> struct List
	{
		template <typename LAST>
		struct Appended { using Result = List<ARGS..., LAST>; };
	};

	template <typename T1, typename T2>
	struct Same { static constexpr bool VALUE = false; };

	template <typename T>
	struct Same<T, T> { static constexpr bool VALUE = true; };

} } /* namespace Dialog::Meta */



struct Dialog::Id
{
	using Value = String<20>;

	Value value;

	bool operator == (Id const &other) const { return value == other.value; }

	bool valid() const { return value.length() > 1; }

	void print(Output &out) const { Genode::print(out, value); }

	static Id from_xml(Xml_node const &node)
	{
		return Id { node.attribute_value("name", Value()) };
	}
};


struct Dialog::Event : Noncopyable
{
	/**
	 * ID of input-event sequence
	 *
	 * A sequence number refers to a sequence of consecutive events that
	 * belong together, e.g., all key events occurring while one key is held,
	 * or all touch motions while keeping the display touched.
	 */
	struct Seq_number
	{
		unsigned value;

		bool operator == (Seq_number const &other) const { return value == other.value; }
	};

	struct Dragged
	{
		bool value;  /* true after click and before clack */
	};

	Seq_number const seq_number;

	Input::Event const event;

	Event(Seq_number seq_number, Input::Event event)
	: seq_number(seq_number), event(event) { }

	void print(Output &out) const
	{
		Genode::print(out, seq_number.value, " ", event);
	}
};


struct Dialog::At : Noncopyable
{
	Event::Seq_number const seq_number;

	Xml_node const &_location; /* widget hierarchy as found in hover reports */

	bool const _valid = _location.has_attribute("name");

	At(Event::Seq_number const seq_number, Xml_node const &location)
	: seq_number(seq_number), _location(location) { }

	/*
	 * The last element is not interpreted as widget type. It is preserved
	 * to denote the type of a 'Scoped' sub dialog.
	 */
	template <typename...>
	struct Narrowed;

	template <typename HEAD, typename... TAIL>
	struct Narrowed<HEAD, TAIL...>
	{
		template <typename AT, typename FN>
		static void with_at(AT const &at, FN const &fn)
		{
			HEAD::with_narrowed_at(at, [&] (AT const &narrowed_at) {
				Narrowed<TAIL...>::with_at(narrowed_at, fn); });
		}
	};

	template <typename LAST_IGNORED>
	struct Narrowed<LAST_IGNORED>
	{
		template <typename AT, typename FN>
		static void with_at(AT const &at, FN const &fn) { fn(at); }
	};

	template <typename... ARGS>
	Id matching_id() const
	{
		struct Ignored { };
		Id result { };
		Narrowed<ARGS..., Ignored>::with_at(*this, [&] (At const &at) {
			result = Id::from_xml(at._location); });
		return result;
	}

	template <typename... HIERARCHY>
	bool matches(Id const &id) const
	{
		return matching_id<HIERARCHY...>().value == id.value;
	}

	bool matches(Event::Seq_number const &s) const
	{
		return s.value == seq_number.value;
	}

	Id id() const { return Id::from_xml(_location); }

	void print(Output &out) const { Genode::print(out, _location); }
};


struct Dialog::Clicked_at : At { using At::At; };
struct Dialog::Clacked_at : At { using At::At; };
struct Dialog::Dragged_at : At { using At::At; };
struct Dialog::Hovered_at : At { using At::At; };


static inline Dialog::Clicked_at const &Dialog::clicked_at(At const &at)
{
	return static_cast<Clicked_at const &>(at);
}


/**
 * Tag for marking types as sub scopes
 *
 * This is a precaution to detect the use of wrong types as 'Scope::sub_scope'
 * argument.
 */
class Dialog::Sub_scope
{
	private: Sub_scope(); /* sub scopes cannot be instantiated */
};


template <typename... HIERARCHY>
struct Dialog::Scope : Noncopyable
{
	using Hierarchy = Meta::List<HIERARCHY...>;

	Id const id;

	Xml_generator &xml;

	At const &hover;

	Event::Dragged const _dragged;

	unsigned _sub_scope_count = 0;

	Scope(Xml_generator &xml, At const &hover, Event::Dragged const dragged, Id const id)
	: id(id), xml(xml), hover(hover), _dragged(dragged) { }

	bool dragged() const { return _dragged.value; };

	template <typename T, typename... ARGS>
	void sub_scope(Id const id, ARGS &&... args)
	{
		/* create new 'Scope' type with 'T' appended */
		using Sub_scope = Scope<HIERARCHY..., T>;

		bool generated = false;

		/* narrow hover information according to sub-scope type */
		T::with_narrowed_at(hover, [&] (At const &narrowed_hover) {
			if (id == Id::from_xml(narrowed_hover._location)) {
				Sub_scope sub_scope { xml, narrowed_hover, _dragged, id };
				T::view_sub_scope(sub_scope, args...);
				generated = true;
			}
		});
		if (generated)
			return;

		static Xml_node unhovered_xml { "<hover/>" };
		At const unhovered_at { hover.seq_number, unhovered_xml };
		Sub_scope sub_scope { xml, unhovered_at, _dragged, id };
		T::view_sub_scope(sub_scope, args...);
	}

	template <typename T, typename... ARGS>
	void sub_scope(ARGS &&... args)
	{
		static_assert(static_cast<Sub_scope *>((T *)(nullptr)) == nullptr,
		              "sub_scope called with type that is not a 'Sub_scope'");

		sub_scope<T>(Id{_sub_scope_count++}, args...);
	}

	template <typename HOSTED, typename... ARGS>
	void widget(HOSTED const &hosted, ARGS &&... args)
	{
		hosted._view_hosted(*this, args...);
	}

	template <typename... ARGS>
	bool hovered(Id const &id) const
	{
		return hover.matching_id<ARGS...>() == id;
	}

	bool hovered() const { return hover._valid; }

	template <typename TYPE, typename FN>
	void node(TYPE const &type, FN const &fn)
	{
		xml.node(type, [&] {
			xml.attribute("name", id.value);
			fn();
		});
	}

	template <typename TYPE, typename FN>
	void sub_node(TYPE const &type, FN const &fn)
	{
		xml.node(type, [&] { fn(); });
	}

	template <typename TYPE, typename NAME, typename FN>
	void named_sub_node(TYPE const &type, NAME const &name, FN const &fn)
	{
		xml.node(type, [&] {
			xml.attribute("name", name);
			fn(); });
	}

	template <typename NAME, typename VALUE>
	void attribute(NAME const &name, VALUE const &value)
	{
		xml.attribute(name, value);
	}

	template <typename FN>
	void as_new_scope(FN const &fn) { fn(*reinterpret_cast<Scope<>*>(this)); }
};


template <typename COMPOUND_SUB_SCOPE>
struct Dialog::Widget : Noncopyable
{
	static_assert(static_cast<Sub_scope *>((COMPOUND_SUB_SCOPE *)(nullptr)) == nullptr,
	              "'Widget' argument must be 'Sub_scope' type");

	using Compound_sub_scope = COMPOUND_SUB_SCOPE;

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		Compound_sub_scope::with_narrowed_at(at, fn); };
};


template <typename COMPOUND_SUB_SCOPE>
struct Dialog::Widget_interface : Noncopyable, Interface
{
	using Compound_sub_scope = COMPOUND_SUB_SCOPE;

	template <typename AT, typename FN>
	static void with_narrowed_at(AT const &at, FN const &fn) {
		Compound_sub_scope::with_narrowed_at(at, fn); };
};


template <typename... HIERARCHY>
struct Dialog::Hosted : Meta::Last<HIERARCHY...>::Type
{
	Id const id;

	using Widget             = typename Meta::Last<HIERARCHY...>::Type;
	using Compound_sub_scope = typename Widget::Compound_sub_scope;

	template <typename... ARGS>
	Hosted(Id const &id, ARGS &&... args) : Widget(args...), id(id) { }

	/*
	 * \noapi helper for 'propagate' methods
	 */
	template <typename AT, typename FN>
	void _with_narrowed_at(AT const &at, FN const &fn) const
	{
		At::Narrowed<HIERARCHY...>::with_at(at, [&] (AT const &narrowed) {
			if (narrowed.template matches<Compound_sub_scope>(id))
				fn(narrowed); });
	}

	void propagate(Clicked_at const &at, auto &&... args) {
		_with_narrowed_at(at, [&] (auto const &at) { this->click(at, args...); }); }

	void propagate(Clicked_at const &at, auto &&... args) const {
		_with_narrowed_at(at, [&] (auto const &at) { this->click(at, args...); }); }

	void propagate(Clacked_at const &at, auto &&... args) {
		_with_narrowed_at(at, [&] (auto const &at) { this->clack(at, args...); }); }

	void propagate(Clacked_at const &at, auto &&... args) const {
		_with_narrowed_at(at, [&] (auto const &at) { this->clack(at, args...); }); }

	void propagate(Dragged_at const &at, auto &&... args) {
		_with_narrowed_at(at, [&] (auto const &at) { this->drag(at, args...); }); }

	void propagate(Dragged_at const &at, auto &&... args) const {
		_with_narrowed_at(at, [&] (auto const &at) { this->drag(at, args...); }); }

	bool if_hovered(Hovered_at const &at, auto const &fn) const
	{
		bool result = false;
		_with_narrowed_at(at, [&] (Hovered_at const &at) {
			result = !at._location.has_type("empty") && fn(at); });
		return result;
	}

	/*
	 * \noapi used internally by 'Scope::widget'
	 */
	template <typename SCOPE, typename... ARGS>
	void _view_hosted(SCOPE &scope, ARGS &&... args) const
	{
		using Call_structure = typename SCOPE::Hierarchy::Appended<Widget>::Result;

		constexpr bool call_structure_matches_scoped_hierarchy =
			Meta::Same<Meta::List<HIERARCHY...>, Call_structure>::VALUE;

		static_assert(call_structure_matches_scoped_hierarchy,
		              "'view' call structure contradicts 'Scoped' hierarchy");

		scope.as_new_scope([&] (Scope<> &s) {
			s.sub_scope<Compound_sub_scope>(id, [&] (Scope<Compound_sub_scope> &s) {
				Widget::view(s, args...); }); });
	}
};


struct Dialog::Top_level_dialog : Interface, Noncopyable
{
	using Name = String<20>;
	Name const name;

	Top_level_dialog(Name const &name) : name(name) { }

	virtual void view(Scope<> &) const = 0;

	virtual void click(Clicked_at const &) { };
	virtual void clack(Clacked_at const &) { };
	virtual void drag (Dragged_at const &) { };
};

#endif /* _INCLUDE__DIALOG__TYPES_H_ */
