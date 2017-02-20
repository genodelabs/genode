/*
 * \brief   Scout configuration
 * \date    2005-11-10
 * \author  Norman Feske <norman.feske@genode-labs.com>
 */

/*
 * Copyright (C) 2005-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

namespace Scout { struct Config; }


struct Scout::Config
{
	bool background_detail = 1;
	bool mouse_cursor      = 0;
	int browser_attr       = 7;
};

#endif /* _CONFIG_H_ */
