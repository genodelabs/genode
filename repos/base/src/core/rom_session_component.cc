/*
 * \brief  Core implementation of the ROM session interface
 * \author Norman Feske
 * \date   2006-07-06
 */

/*
 * Copyright (C) 2006-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/arg_string.h>
#include <root/root.h>

/* core includes */
#include <rom_session_component.h>

using namespace Core;


Rom_session_component::Rom_session_component(Rom_fs         &rom_fs,
                                             Rpc_entrypoint &ds_ep,
                                             const char     *args)
{
	rom_fs.with_element(label_from_args(args).last_element(),
		[&] (Rom_module const &rom) {
			_ds.construct(ds_ep, rom.size, rom.addr, CACHED, false, nullptr); },
		[&] { });
}
