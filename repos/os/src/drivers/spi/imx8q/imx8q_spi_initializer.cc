/*
 * \brief  Imx8q based spi initializer
 * \author Jean-Adrien Domage <jean-adrien.domage@gapfruit.com>
 * \date   2021-04-21
 */

/*
 * Copyright (C) 2013-2021 Genode Labs GmbH
 * Copyright (C) 2021 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <spi_driver.h>
#include "ecspi/ecspi_driver.h"

Spi::Ecspi_driver::Config Parse_ecspi_config(Genode::Xml_node const &node) {
	return {
		.verbose       = node.attribute_value("verbose", false),
		.loopback      = node.attribute_value("loopback", false),
		.clock_divider = node.attribute_value("clock_divider", static_cast<Genode::uint8_t>(0)),
		.timeout       = node.attribute_value("timeout", static_cast<Genode::uint64_t>(1000)),
	};
}

/*
 * On Imx8q_evk only ecspi with FIFO based transfer is implemented.
 * Update the initializer with new implementation (e.g ecspi with dma transfer, or qspi)
 * allocated based on the config.
 */
Spi::Driver* Spi::initialize(Genode::Env &env, const Xml_node &node) {
	static Ecspi_driver driver { env, Parse_ecspi_config(node) };
	return &driver;
}
