/*
 * \brief  Decorator configuration handling
 * \author Norman Feske
 * \date   2015-09-17
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

/* Genode includes */
#include <os/session_policy.h>
#include <util/color.h>

/* decorator includes */
#include <decorator/types.h>

namespace Decorator {

	class Config;

	typedef Genode::String<200> Window_title;
}


class Decorator::Config
{
	public:

		class Window_control
		{
			public:

				enum Type { TYPE_CLOSER, TYPE_TITLE, TYPE_MAXIMIZER,
				            TYPE_MINIMIZER, TYPE_UNMAXIMIZER, TYPE_UNDEFINED };

				enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

			private:

				Type  _type  = TYPE_UNDEFINED;
				Align _align = ALIGN_CENTER;

			public:

				Window_control() { }

				Window_control(Type type, Align align)
				: _type(type), _align(align) { }

				Type  type()  const { return _type; }
				Align align() const { return _align; }

				static char const *type_name(Type type)
				{
					switch (type) {
					case TYPE_CLOSER:      return "closer";
					case TYPE_TITLE:       return "title";
					case TYPE_MAXIMIZER:   return "maximizer";
					case TYPE_MINIMIZER:   return "minimizer";
					case TYPE_UNMAXIMIZER: return "unmaximizer";
					case TYPE_UNDEFINED:   return "undefined";
					};
					return "";
				}

				bool operator != (Window_control const &other) const
				{
					return _type != other._type || _align != other._align;
				}
		};

	private:

		Genode::Allocator &_alloc;

		/**
		 * Maximum number of configured window controls
		 */
		enum { MAX_WINDOW_CONTROLS = 10 };

		/**
		 * Array of window elements
		 */
		Window_control *_window_controls[MAX_WINDOW_CONTROLS];

		unsigned _num_window_controls = 0;

		void _reset_window_controls()
		{
			for (unsigned i = 0; i < MAX_WINDOW_CONTROLS; i++) {
				if (_window_controls[i]) {
					Genode::destroy(_alloc, _window_controls[i]);
					_window_controls[i] = nullptr;
				}
			}
			_num_window_controls = 0;
		}

	public:

		Config(Genode::Allocator &alloc) : _alloc(alloc)
		{
			_reset_window_controls();
		}

		/**
		 * Exception type
		 */
		class Index_out_of_range { };

		/**
		 * Return information about the Nth window control
		 *
		 * The index 'n' denotes the position of the window control from left
		 * to right.
		 *
		 * \throw Index_out_of_range
		 */
		Window_control window_control(unsigned n) const
		{
			/* return title of no controls are configured */
			if (_num_window_controls == 0 && n == 0)
				return Window_control(Window_control::TYPE_TITLE,
				                      Window_control::ALIGN_CENTER);

			if (n >= MAX_WINDOW_CONTROLS || !_window_controls[n])
				throw Index_out_of_range();

			return *_window_controls[n];
		}

		unsigned num_window_controls() const
		{
			/*
			 * We always report at least one window control. Even if none
			 * was configured, we present a title.
			 */
			return Genode::max(_num_window_controls, 1UL);
		}

		/**
		 * Return the base color of the window with the specified title
		 */
		Color base_color(Window_title const &title) const
		{
			Color result(68, 75, 95);

			try {
				Genode::Session_policy policy(title);
				result = policy.attribute_value("color", result);

			} catch (Genode::Session_policy::No_policy_defined) { }

			return result;
		}

		/**
		 * Return gradient intensity in percent
		 */
		int gradient_percent(Window_title const &title) const
		{
			unsigned long result =
				Genode::config()->xml_node().attribute_value("gradient", 32UL);
			
			try {
				Genode::Session_policy policy(title);
				result = policy.attribute_value("gradient", result);

			} catch (Genode::Session_policy::No_policy_defined) { }

			return result;
		}

		/**
		 * Update the internally cached configuration state
		 */
		void update()
		{
			_reset_window_controls();

			/*
			 * Parse configuration of window controls
			 */
			auto lambda = [&] (Xml_node node) {

				if (_num_window_controls >= MAX_WINDOW_CONTROLS) {
					PWRN("number of configured window controls exceeds maximum");
					return;
				}

				Window_control::Type  type  = Window_control::TYPE_UNDEFINED;
				Window_control::Align align = Window_control::ALIGN_CENTER;

				if (node.has_type("title"))     type = Window_control::TYPE_TITLE;
				if (node.has_type("closer"))    type = Window_control::TYPE_CLOSER;
				if (node.has_type("maximizer")) type = Window_control::TYPE_MAXIMIZER;
				if (node.has_type("minimizer")) type = Window_control::TYPE_MINIMIZER;

				if (node.has_attribute("align")) {
					Genode::Xml_attribute attr = node.attribute("align");
					if (attr.has_value("left"))  align = Window_control::ALIGN_LEFT;
					if (attr.has_value("right")) align = Window_control::ALIGN_RIGHT;
				}

				_window_controls[_num_window_controls++] =
					new (_alloc) Window_control(type, align);
			};

			Xml_node config = Genode::config()->xml_node();

			try { config.sub_node("controls").for_each_sub_node(lambda); }
			catch (Xml_node::Nonexistent_sub_node) { }
		}
};

#endif /* _CONFIG_H_ */
