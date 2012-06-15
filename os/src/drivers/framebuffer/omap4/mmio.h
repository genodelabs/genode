/*
 * \brief  Utilities for accessing MMIO registers
 * \author Norman Feske
 * \date   2012-06-15
 */

#ifndef _MMIO_H_
#define _MMIO_H_

#include <base/printf.h>

/* Genode includes */
#include <util/mmio.h>

struct Delayer
{
	virtual void usleep(unsigned us) = 0;
};

/**
 * Extend 'Genode::Mmio' framework with the ability to poll for bitfield states
 */
struct Mmio : Genode::Mmio
{
	template <typename BITFIELD>
	inline bool wait_for(typename BITFIELD::Compound_reg::access_t const value,
	                     Delayer &delayer,
	                     unsigned max_attempts = 500,
	                     unsigned delay_us     = 1000)
	{
		for (unsigned i = 0; i < max_attempts; i++, delayer.usleep(delay_us))
			if (read<BITFIELD>() == value)
				return true;
	
		return false;
	}

	Mmio(Genode::addr_t mmio_base) : Genode::Mmio(mmio_base) { }
};


#endif /* _MMIO_H_ */
