/*
 * \brief  Initialization of sub-modules
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2020-10-14
 */

/*
 * Copyright (C) 2020-2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _INIT_H_
#define _INIT_H_

namespace Genode { struct Env; }

namespace Sup { void init(Genode::Env &); }

namespace Pthread { void init(Genode::Env &); }

namespace Network { void init(Genode::Env &); }

namespace Xhci { void init(Genode::Env &); }

namespace Services { void init(Genode::Env &); }

#endif /* _INIT_H_ */
