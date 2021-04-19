/*
 * \brief  Test spi driver with a shift register
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

#include <base/component.h>
#include <util/string.h>
#include <spi_session/connection.h>

namespace Spi {

	using namespace Genode;

	struct Main;

}

struct Spi::Main
{
private:

	static constexpr char const _small_label[] = { "Hello friend!" };

	static constexpr char const _read_after_write_overlape[] = {
		"ABCD************************************************************"
		"************************************************************WXYZ"
		"F"
	};

	static constexpr char const _1k_label[] {
		"ABCD************************************************************"
		"****************************************************************"
		"****************************************************************"
		"****************************************************************"
		"****************************************************************"
		"****************************************************************"
		"****************************************************************"
		"************************************************************WXYZ"
	};

	Genode::Env    &_env;
	Spi::Connection _ecspi_device { _env, sizeof(_1k_label) };

	public:


	bool _test_transaction(char const *data, size_t data_size) {

		/* create a new buffer with reference data to transfer through the spi bus */
		char buffer[data_size];
		memcpy(buffer, data, data_size);

		/* perform the transfer */
		size_t bytes_transferred = _ecspi_device.transfer(buffer, data_size);

		/* check that the transfer complete successfully */
		if (bytes_transferred != data_size) {
			error("Transfer did not complete successfully.");
			return false;
		}

		/* Check that the transferred data has not been altered */
		if (auto idx = Genode::strcmp(data, buffer, data_size) != 0) {
			error("Error bytes read differ from byte written!");
			error("Expect: ", data[idx], "  Got: ", buffer[idx], "  at idx: ", idx);
			return false;
		} else {
			log("Buffer: ", Genode::Cstring { buffer });
		}
		return true;
	}


	bool _test_expect_exception(size_t data_size) {
		char buffer[data_size];

		try {
			_ecspi_device.transfer(buffer, data_size);
		} catch(...) {
			return true;
		}
		return false;
	}

	explicit Main(Genode::Env& env)
	:
		_env(env)
	{
			bool result = true;

		try {

			result &= _test_transaction(_small_label, sizeof(_small_label));
			result &= _test_transaction(_read_after_write_overlape, sizeof(_read_after_write_overlape));

			_ecspi_device.settings({
			    .mode                  = 0x2,
			    .clock_idle_state      = Settings::STATE_LOW,
			    .data_lines_idle_state = Settings::STATE_HIGH,
			    .ss_line_active_state  = Settings::STATE_LOW,
			});

			result &= _test_transaction(_1k_label, sizeof(_1k_label));

			result &= _test_expect_exception(8192);

		} catch (...) {
			error("Spi::Bus_error exception");
			result = false;
		}

		if (!result) {
			log("Shift register test failed!");
		} else {
			log("Shift register test succeed!");
		}
	}
};

void Component::construct(Genode::Env &env) { static Spi::Main main(env); }
