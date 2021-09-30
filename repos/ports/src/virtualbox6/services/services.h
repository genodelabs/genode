/*
 * \brief  Service backend
 * \author Christian Helmuth
 * \date   2021-09-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#ifndef _SERVICES__SERVICES_H_
#define _SERVICES__SERVICES_H_

namespace Genode { struct Env; }

namespace Services {

	void init(Genode::Env &);

	Genode::Env & env();
}

#endif /* _SERVICES__SERVICES_H_ */
