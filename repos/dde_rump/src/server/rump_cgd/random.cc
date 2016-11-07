/**
 * \brief  Add random support for CGD
 * \author Sebastian Sumpf
 * \date   2015-02-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <util/random.h>

extern "C" {
	namespace Jitter {
		#include <jitterentropy.h>
	}
}

typedef Genode::size_t size_t;


/***********************************
 ** Jitter entropy for randomness **
 ***********************************/

struct Entropy
{
	struct Jitter::rand_data *ec_stir;

	Entropy()
	{
		Jitter::jent_entropy_init();
		ec_stir = Jitter::jent_entropy_collector_alloc(0, 0);
	}

	static Entropy *e()
	{
		static Entropy _e;
		return &_e;
	}

	size_t read(char *buf, size_t len)
	{
		int err;
		if ((err = Jitter::jent_read_entropy(ec_stir, buf, len) < 0)) {
			Genode::error("failed to read entropy: ", err);
			return 0;
		}

		return len;
	}
};


int rumpuser_getrandom_backend(void *buf, size_t buflen, int flags, __SIZE_TYPE__ *retp)
{
	*retp = Entropy::e()->read((char *)buf, buflen);
	*retp = buflen;
	return 0;
}
