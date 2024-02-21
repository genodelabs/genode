/*
 * \brief  Conditionally visible widget
 * \author Norman Feske
 * \date   2022-05-20
 *
 * Float widget that generates its content only if a given condition is true.
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__CONDITIONAL_FLOAT_WIDGET_H_
#define _VIEW__CONDITIONAL_FLOAT_WIDGET_H_

#include <view/dialog.h>

namespace Sculpt { template <typename> struct Conditional_float_widget; }


template <typename WIDGET>
struct Sculpt::Conditional_float_widget : Widget<Float>
{
	Id const id;

	bool const _centered;

	Hosted<Float, WIDGET> hosted;

	void view(Scope<Float> &s, bool condition, auto &&... args) const
	{
		if (!_centered) {
			s.attribute("east", "yes");
			s.attribute("west", "yes");
		}
		if (condition)
			s.widget(hosted, args...);
	}

	void click(Clicked_at const &at, auto &&... args)       { hosted.propagate(at, args...); }
	void click(Clicked_at const &at, auto &&... args) const { hosted.propagate(at, args...); }
	void clack(Clacked_at const &at, auto &&... args)       { hosted.propagate(at, args...); }
	void clack(Clacked_at const &at, auto &&... args) const { hosted.propagate(at, args...); }
	void drag (Dragged_at const &at, auto &&... args)       { hosted.propagate(at, args...); }
	void drag (Dragged_at const &at, auto &&... args) const { hosted.propagate(at, args...); }

	struct Attr { bool centered; };

	Conditional_float_widget(Attr const &attr, Id const &id, auto &&... args)
	:
		id(id), _centered(attr.centered), hosted(id, args...)
	{ }
};


namespace Sculpt {

	template <typename WIDGET>
	struct Conditional_widget : Hosted<Vbox, Conditional_float_widget<WIDGET> >
	{
		using Attr = Conditional_float_widget<WIDGET>::Attr;

		Conditional_widget(Attr const &attr, Id const &id, auto &&... args)
		:
			/*
			 * The first 'id' argument is held at 'Hosted', the second 'id'
			 * argument is held by 'Conditional_float_widget'. The 'args' are
			 * forwarded to 'WIDGET'.
			 */
			Hosted<Vbox, Conditional_float_widget<WIDGET> >(id, attr, id, args...)
		{ }

		Conditional_widget(Id const &id, auto &&... args)
		:
			Conditional_widget(Attr { .centered = false }, id, args...)
		{ }
	};
}

#endif /* _VIEW__CONDITIONAL_FLOAT_WIDGET_H_ */
