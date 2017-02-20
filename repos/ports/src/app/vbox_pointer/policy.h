/*
 * \brief  VirtualBox pointer policies
 * \author Christian Helmuth
 * \date   2015-06-08
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VBOX_POINTER_POLICY_H_
#define _VBOX_POINTER_POLICY_H_

/* Genode includes */
#include <os/pixel_rgb565.h>
#include <nitpicker_session/nitpicker_session.h>
#include <vbox_pointer/shape_report.h>
#include <util/list.h>

/* local includes */
#include "util.h"


namespace Vbox_pointer {
	struct Pointer_updater;
	struct Policy;
	struct Policy_entry;
	struct Policy_registry;
}


struct Vbox_pointer::Pointer_updater
{
	virtual void update_pointer(Policy &initiator) = 0;
};


struct Vbox_pointer::Policy
{
	virtual Nitpicker::Area shape_size() const = 0;
	virtual Nitpicker::Point shape_hot() const = 0;
	virtual bool shape_valid() const           = 0;

	virtual void draw_shape(Genode::Pixel_rgb565 *pixel) = 0;
};


class Vbox_pointer::Policy_registry : private Genode::List<Policy_entry>
{
	private:

		Pointer_updater   &_updater;
		Genode::Env       &_env;
		Genode::Allocator &_alloc;

	public:

		Policy_registry(Pointer_updater &updater, Genode::Env &env,
		                Genode::Allocator &alloc)
		: _updater(updater), _env(env), _alloc(alloc) { }

		void update(Genode::Xml_node config);

		Policy * lookup(String const &label, String const &domain);
};

#endif /* _VBOX_POINTER_POLICY_H_ */
