/*
 * \brief  SPI session capability type
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-28
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


#ifndef _INCLUDE__SPI_SESSION__CAPABILITY_H_
#define _INCLUDE__SPI_SESSION__CAPABILITY_H_

#include <base/capability.h>
#include <spi_session/spi_session.h>

namespace Spi {

	using Session_capability = Genode::Capability<Session>;

}

#endif // _INCLUDE__SPI_SESSION__CAPABILITY_H_
