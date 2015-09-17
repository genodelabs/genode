/*
 * \brief  Nitpicker session interface
 * \author Norman Feske
 * \date   2006-08-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SESSION_H_
#define _SESSION_H_

/* Genode includes */
#include <util/list.h>
#include <util/string.h>
#include <os/session_policy.h>

/* local includes */
#include "color.h"
#include "canvas.h"
#include "domain_registry.h"

class View;
class Session;

namespace Input { class Event; }

typedef Genode::List<Session> Session_list;

class Session : public Session_list::Element
{
	private:

		Genode::Session_label  const  _label;
		Domain_registry::Entry const *_domain;
		Texture_base           const *_texture = { 0 };
		bool                          _uses_alpha = { false };
		bool                          _visible = true;
		View                         *_background = 0;
		unsigned char          const *_input_mask = { 0 };

	public:

		/**
		 * Constructor
		 *
		 * \param label  session label
		 */
		explicit Session(Genode::Session_label const &label) : _label(label) { }

		virtual ~Session() { }

		virtual void submit_input_event(Input::Event ev) = 0;

		virtual void submit_sync() = 0;

		Genode::Session_label const &label() const { return _label; }

		/**
		 * Return true if session label starts with specified 'selector'
		 */
		bool matches_session_label(char const *selector) const
		{
			/*
			 * Append label separator to match selectors with a trailing
			 * separator.
			 */
			char label[Genode::Session_label::capacity() + 4];
			Genode::snprintf(label, sizeof(label), "%s ->", _label.string());
			return Genode::strcmp(label, selector,
			                      Genode::strlen(selector)) == 0;
		}

		bool xray_opaque() const { return _domain && _domain->xray_opaque(); }

		bool xray_no() const { return _domain && _domain->xray_no(); }

		bool origin_pointer() const { return _domain && _domain->origin_pointer(); }

		unsigned layer() const { return _domain ? _domain->layer() : ~0UL; }

		bool visible() const { return _visible; }

		void visible(bool visible) { _visible = visible; }

		Domain_registry::Entry::Name domain_name() const
		{
			return _domain ? _domain->name() : Domain_registry::Entry::Name();
		}

		Texture_base const *texture() const { return _texture; }

		void texture(Texture_base const *texture, bool uses_alpha)
		{
			_texture    = texture;
			_uses_alpha = uses_alpha;
		}

		/**
		 * Set input mask buffer
		 *
		 * \param mask  input mask buffer containing a byte value per texture
		 *              pixel, which describes the policy of handling user
		 *              input referring to the pixel. If set to zero, the input
		 *              is passed through the view such that it can be handled
		 *              by one of the subsequent views in the view stack. If
		 *              set to one, the input is consumed by the view. If
		 *              'input_mask' is a null pointer, user input is
		 *              unconditionally consumed by the view.
		 */
		void input_mask(unsigned char const *mask) { _input_mask = mask; }

		Color color() const { return _domain ? _domain->color() : WHITE; }

		View *background() const { return _background; }

		void background(View *background) { _background = background; }

		/**
		 * Return true if session uses an alpha channel
		 */
		bool uses_alpha() const { return _texture && _uses_alpha; }

		/**
		 * Calculate session-local coordinate to physical screen position
		 *
		 * \param pos          coordinate in session-local coordinate system
		 * \param screen_area  session-local screen size
		 */
		Point phys_pos(Point pos, Area screen_area) const
		{
			return _domain ? _domain->phys_pos(pos, screen_area) : Point(0, 0);
		}

		/**
		 * Return session-local screen area
		 *
		 * \param phys_pos  size of physical screen
		 */
		Area screen_area(Area phys_area) const
		{
			return _domain ? _domain->screen_area(phys_area) : Area(0, 0);
		}

		/**
		 * Return input mask value at specified buffer position
		 */
		unsigned char input_mask_at(Point p) const
		{
			if (!_input_mask || !_texture) return 0;

			/* check boundaries */
			if ((unsigned)p.x() >= _texture->size().w()
			 || (unsigned)p.y() >= _texture->size().h())
				return 0;

			return _input_mask[p.y()*_texture->size().w() + p.x()];
		}

		bool has_same_domain(Session const *s) const
		{
			return s && (s->_domain == _domain);
		}

		bool has_valid_domain() const
		{
			return _domain;
		}

		void reset_domain()
		{
			_domain = nullptr;
		}

		/**
		 * Set session domain according to the list of configured policies
		 *
		 * Select the policy that matches the label. If multiple policies
		 * match, select the one with the largest number of characters.
		 */
		void apply_session_policy(Domain_registry const &domain_registry)
		{
			reset_domain();

			try {
				Genode::Session_policy policy(_label);

				/* read domain attribute */
				if (!policy.has_attribute("domain")) {
					PERR("policy for label \"%s\" lacks domain declaration",
					     _label.string());
					return;
				}

				typedef Domain_registry::Entry::Name Domain_name;
				char buf[sizeof(Domain_name)];
				buf[0] = 0;
				try {
					policy.attribute("domain").value(buf, sizeof(buf)); }
				catch (...) { }

				Domain_name name(buf);
				_domain = domain_registry.lookup(name);

			} catch (...) { }
		}
};

#endif
