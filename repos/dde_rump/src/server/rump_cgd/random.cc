/**
 * \brief  Add random support for CGD
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2015-02-13
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* local rump includes */
#include <rump/env.h>
#include <util/random.h>

/* library includes */
#include <jitterentropy.h>

typedef Genode::size_t size_t;


/***********************************
 ** Jitter entropy for randomness **
 ***********************************/

struct Entropy
{
	struct rand_data *ec_stir;

	struct Initialization_failed : Genode::Exception { };

	Entropy(Genode::Allocator &alloc)
	{
		jitterentropy_init(alloc);

		int err = jent_entropy_init();
		if (err) {
			Genode::error("could not initialize jitterentropy library");
			throw Initialization_failed();
		}

		ec_stir = jent_entropy_collector_alloc(0, 0);
		if (ec_stir == nullptr) {
			Genode::error("could not initialize jitterentropy library");
			throw Initialization_failed();
		}
	}

	size_t read(char *buf, size_t len)
	{
		int err;
		if ((err = jent_read_entropy(ec_stir, buf, len) < 0)) {
			Genode::error("failed to read entropy: ", err);
			return 0;
		}

		return len;
	}
};


static Genode::Constructible<Entropy> _entropy;
static bool                           _init_failed;


int rumpuser_getrandom_backend(void *buf, size_t buflen, int flags, Genode::size_t *retp)
{
	if (!_entropy.constructed()) {
		if (_init_failed) {
			*retp = 0;
			return -1;
		}

		try { _entropy.construct(Rump::env().heap()); }
		catch (Entropy::Initialization_failed) {
			_init_failed = true;
			return -1;
		}
	}

	*retp = _entropy->read((char *)buf, buflen);
	return 0;
}
