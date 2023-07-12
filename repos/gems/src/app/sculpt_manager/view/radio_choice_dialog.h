/*
 * \brief  Radio-button dialog
 * \author Norman Feske
 * \date   2021-03-19
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__RADIO_CHOICE_DIALOG_H_
#define _VIEW__RADIO_CHOICE_DIALOG_H_

#include <view/dialog.h>

namespace Sculpt { struct Radio_choice_dialog; }


struct Sculpt::Radio_choice_dialog : Noncopyable, Deprecated_dialog
{
	typedef Hoverable_item::Id Id;

	Id const _id;

	struct Min_ex { unsigned left; unsigned right; };

	Min_ex const _min_ex;

	Hoverable_item _choice_item { };

	bool _unfolded = false;

	Hover_result hover(Xml_node hover) override
	{
		return any_hover_changed(_choice_item.match(hover, "hbox", "frame", "vbox", "hbox", "name"));
	}

	void reset() override
	{
		_unfolded = false;
	}

	struct Choice : Interface, Noncopyable
	{
		virtual void generate(Id const &option_id) const = 0;
	};

	void generate(Xml_generator &) const override { };

	template <typename FN>
	void generate(Xml_generator &xml, Id const &selected_id, FN const &fn) const
	{
		struct Choice_generator : Choice
		{
			Xml_generator &xml;

			Radio_choice_dialog const &dialog;

			Id const selected_id;

			Choice_generator(Xml_generator &xml, Id const &selected_id,
			                 Radio_choice_dialog const &dialog)
			:
				xml(xml), dialog(dialog), selected_id(selected_id)
			{ }

			void generate(Id const &option_id) const override
			{
				bool const selected = (option_id == selected_id);

				if (!selected && !dialog._unfolded)
					return;

				gen_named_node(xml, "hbox", option_id, [&] () {

					gen_named_node(xml, "float", "left", [&] () {
						xml.attribute("west", "yes");

						xml.node("hbox", [&] () {
							gen_named_node(xml, "button", "button", [&] () {

								if (selected)
									xml.attribute("selected", "yes");

								xml.attribute("style", "radio");
								dialog._choice_item.gen_hovered_attr(xml, option_id);
								xml.node("hbox", [&] () { });
							});
							gen_named_node(xml, "label", "name", [&] () {
								xml.attribute("text", Path(" ", option_id)); });
						});
					});

					gen_named_node(xml, "hbox", "right", [&] () { });
				});
			}

		} choice_generator { xml, selected_id, *this };

		gen_named_node(xml, "hbox", _id, [&] () {
			gen_named_node(xml, "vbox", "left", [&] () {

				gen_named_node(xml, "float", "title", [&] () {
					xml.attribute("north", true);
					xml.attribute("west",  true);
					xml.node("frame", [&] () {
						xml.attribute("style", "invisible");

						xml.node("hbox", [&] () {
							xml.node("label", [&] () {
								xml.attribute("text", _id); }); });

						/* used for consistent vertical text alignment */
						gen_named_node(xml, "button", "vspace", [&] () {
							xml.attribute("style", "invisible");
							xml.node("hbox", [&] () { });
						});
					});
				});

				gen_named_node(xml, "label", "hspace", [&] () {
					xml.attribute("min_ex", _min_ex.left); });
			});

			gen_named_node(xml, "frame", "right", [&] () {
				xml.node("vbox", [&] () {

					fn(choice_generator);

					gen_named_node(xml, "label", "hspace", [&] () {
						xml.attribute("min_ex", _min_ex.right); });
				});
			});
		});
	}

	Click_result click()
	{
		/* unfold choice on click */
		if (_choice_item._hovered == "")
			return Click_result::IGNORED;

		_unfolded = true;
		return Click_result::CONSUMED;
	}

	Id hovered_choice() const { return _choice_item._hovered; }

	Radio_choice_dialog(Id const &id, Min_ex min_ex)
	:
		_id(id), _min_ex(min_ex)
	{ }
};

#endif /* _VIEW__RADIO_CHOICE_DIALOG_H_ */
