/*
 * \brief  Representation of a view owner
 * \author Norman Feske
 * \date   2017-11-16
 *
 * The view owner defines the policy when drawing or interacting with a
 * view. Except for the background and pointer-origin views that are
 * owned by nitpicker, the view owner corresponds to the session that
 * created the view.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW_OWNER_H_
#define _VIEW_OWNER_H_

/* Genode includes */
#include <util/xml_generator.h>
#include <input/event.h>
#include <nitpicker_session/nitpicker_session.h>
#include <os/texture.h>

/* local includes */
#include "types.h"


namespace Nitpicker { class View_owner; }


struct Nitpicker::View_owner
{
	/**
	 * Return the owner's session label
	 */
	virtual Session::Label label() const { return Session::Label(""); }

	virtual bool matches_session_label(Session::Label const &) const { return false; }

	virtual bool visible() const { return true; }

	virtual bool label_visible() const { return false; }

	virtual bool has_same_domain(View_owner const *) const { return false; }

	virtual bool has_focusable_domain() const { return false; }

	virtual bool has_transient_focusable_domain() const { return false; }

	virtual Color color() const { return black(); }

	virtual bool content_client() const { return true; }

	virtual bool hover_always() const { return false; }

	/**
	 * Return true if owner uses an alpha channel
	 */
	virtual bool uses_alpha() const { return false; }

	/**
	 * Return layer assigned to the owner's domain
	 */
	virtual unsigned layer() const { return ~0U; }

	/**
	 * Return true if the owner uses the pointer as coordinate origin
	 */
	virtual bool origin_pointer() const { return false; }

	/**
	 * Return the owner's designated background view
	 */
	virtual View const *background() const { return nullptr; }

	/**
	 * Teturn texture containing the owners virtual frame buffer
	 */
	virtual Texture_base const *texture() const { return nullptr; }

	/**
	 * Return input-mask value at given position
	 */
	virtual unsigned char input_mask_at(Point p) const { return 0; }

	virtual void submit_input_event(Input::Event ev) { }

	/**
	 * Produce report with the owner's information
	 */
	virtual void report(Xml_generator &xml) const { }
};

#endif /* _VIEW_OWNER_H_ */
