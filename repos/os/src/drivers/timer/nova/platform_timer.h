/*
 * \brief  Platform timer using Nova timed semaphore down
 * \author Alexander Boettcher
 * \date   2014-06-24
 */

/*
 * Copyright (C) 2014-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PLATFORM_TIMER_H_
#define _PLATFORM_TIMER_H_

/* Genode includes */
#include <os/attached_rom_dataspace.h>
#include <os/server.h>
#include <trace/timestamp.h>

class Platform_timer
{
	private:

		Genode::addr_t _sem;
		unsigned long _timeout;
		Genode::Trace::Timestamp _tsc_start;
		unsigned long _tsc_khz;

		/* 1 / ((us / (1000 * 1000)) * (tsc_khz * 1000)) */
		enum { TSC_FACTOR = 1000ULL };

		/**
		 * Convenience function to calculate time in us value out of tsc
		 */
		inline unsigned long _time_in_us(unsigned long long tsc,
		                                 bool sub_tsc_start = true) const
		{
			if (sub_tsc_start)
				return (tsc - _tsc_start) / (_tsc_khz / TSC_FACTOR);

			return (tsc) / (_tsc_khz / TSC_FACTOR);
		}
 
	public:

		/**
		 * Constructor
		 */
		Platform_timer()
		:
			_sem(~0UL), _timeout(0),
			_tsc_start(Genode::Trace::timestamp())
		{
			/* read out the tsc frequenzy once */
			Genode::Attached_rom_dataspace _ds("hypervisor_info_page");
			Nova::Hip * const hip = _ds.local_addr<Nova::Hip>();
			_tsc_khz = hip->tsc_freq;
		}


		/**
		 * Return current time-counter value in microseconds
		 */
		unsigned long curr_time() const
		{
			return _time_in_us(Genode::Trace::timestamp());
		}

		/**
		 * Return maximum timeout as supported by the platform
		 */
		unsigned long max_timeout() { return _time_in_us(~0UL); }

		/**
		 * Schedule next timeout
		 *
		 * \param timeout_usec  timeout in microseconds
		 */
		void schedule_timeout(unsigned long timeout_usec)
		{
			using namespace Genode;

			/* check whether to cancel last timeout */
			if (timeout_usec == 0 && _sem != ~0UL) {
				uint8_t res = Nova::sm_ctrl(_sem, Nova::SEMAPHORE_UP);
				if (res != Nova::NOVA_OK)
					nova_die();
			}
				
			/* remember timeout to be set during wait_for_timeout call */
			_timeout = timeout_usec;
		}

		/**
		 * Block for the next scheduled timeout
		 */
		void wait_for_timeout(Genode::Thread_base *blocking_thread)
		{
			using namespace Genode;
			using namespace Nova;

			if (_sem == ~0UL)
				_sem = blocking_thread->tid().exc_pt_sel + SM_SEL_EC;
 
			addr_t sem = _sem;

			/* calculate absolute timeout */
			Trace::Timestamp now   = Trace::timestamp();
			Trace::Timestamp us_64 = _timeout; 

			if (_timeout == max_timeout()) {
				/* tsc_absolute == 0 means blocking without timeout */
				Genode::uint8_t res = sm_ctrl(sem, SEMAPHORE_DOWN, 0);
				if (res != Nova::NOVA_OK && res != Nova::NOVA_TIMEOUT)
					nova_die();
				return;
			}

			/* block until timeout fires or it gets canceled */
			unsigned long long tsc_absolute = now + us_64 * (_tsc_khz / TSC_FACTOR);
			Genode::uint8_t res = sm_ctrl(sem, SEMAPHORE_DOWN, tsc_absolute);
			if (res != Nova::NOVA_OK && res != Nova::NOVA_TIMEOUT)
				nova_die();
		}
};

#endif /* _PLATFORM_TIMER_H_ */
