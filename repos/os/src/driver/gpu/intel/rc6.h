/*
 * \brief  RC6 power stage based on the Linux driver
 * \author Sebastian Sumpf
 * \date   2025-03-11
 *
 * Enable RC6, there is no support for deep and deep deep. When enabled low
 * voltage mode is entered when the GPU goes idle for CHECK_INACTIVEus while
 * power is restored when new workloads are received via 'progress'.
 *
 * Note: Bspec = Behavioral Specification
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RC6_H_
#define _RC6_H_

#include <mmio.h>

namespace Igd {
	class Rc6;
}


class Igd::Rc6
{
	private:

		/* every 2s */
		enum { CHECK_INACTIVE = 2'000'000 };

		Genode::Env &_env;
		Mmio        &_mmio;

		Timer::Connection _rc6_watchdog { _env };

		Mmio::RC_CTRL0::access_t _ctrl { 0 };

		Genode::Signal_handler<Rc6> _timer_sigh { _env.ep(), *this,
			&Rc6::_handle_timer };

		bool _progress  { false };
		bool _suspended { false };

		/* NEEDS_RC6_CTX_CORRUPTION_WA(i915)) */
		bool _pctx_corrupted()
		{
			if (_mmio.generation() != 9) return false;
			if (_mmio.read<Mmio::GEN8_RC6_CTX_INFO>()) return false;

			Genode::warning("RC6 context corruption, disabling RC6");
			return true;
		}

		/* handles render engine only */
		void _gen9_enable()
		{
			if (_mmio.skylake()) {
				/*
				 * WaRsDoubleRc6WrlWithCoarsePowerGating:skl Doubling WRL only
				 * when CPG is enabled
				 */
				_mmio.write<Mmio::RC_WAKE_RATE_LIMIT::Rc6>(108);
			} else
				_mmio.write<Mmio::RC_WAKE_RATE_LIMIT::Rc6>(54);

			_mmio.write<Mmio::RC_EVALUATION_INTERVAL>(125000); /* 12500 * 1280ns */
			_mmio.write<Mmio::RC_IDLE_HYSTERSIS>(25); /* 25 * 1280ns */
			_mmio.write<Mmio::RING_MAX_IDLE>(10);
			_mmio.write<Mmio::RC_WAKE_HYSTERSIS>(0);
			_mmio.write<Mmio::RC_PROMO_TIME>(37500); /* 37.5/125ms per EI */


			/* 2c: Program Coarse Power Gating Policies */
			_mmio.write<Mmio::GEN9_RENDER_PG_IDLE_HYSTERESIS>(250);

			/* 3a: Enable RC6 */
			/*
			 * WaRsDisableCoarsePowerGating:skl,cnl
			 *   - Render/Media PG need to be disabled with RC6.
			 *
			 *  actually just for gt3 and gt4 not for gt2, but we cannot distinguish
			 *  that right now
			 *   if (!_mmio.skylake())
			 *     _mmio.write<Mmio::GEN9_PG_ENABLE::Gen9_render_pg_enable>(1);
			 */

			Mmio::RC_CTRL0::Ei_hw::set(_ctrl, 1);
			Mmio::RC_CTRL0::Rc6_enable::set(_ctrl, 1);
			Mmio::RC_CTRL0::Hw_control_enable::set(_ctrl, 1);
		}

		/* handles render engine only */
		void _gen11_enable()
		{
			/* 2b: Program RC6 thresholds.*/
			_mmio.write<Mmio::RC_WAKE_RATE_LIMIT::Rc6>(54);
			_mmio.write<Mmio::RC_EVALUATION_INTERVAL>(125000); /* 12500 * 1280ns */
			_mmio.write<Mmio::RC_IDLE_HYSTERSIS>(25); /* 25 * 1280ns */
			_mmio.write<Mmio::RING_MAX_IDLE>(10);
			_mmio.write<Mmio::RC_WAKE_HYSTERSIS>(0);
			_mmio.write<Mmio::RC_PROMO_TIME>(50000); /* 50/125ms per EI */

			/* 2c: Program Coarse Power Gating Policies */
			_mmio.write<Mmio::GEN9_RENDER_PG_IDLE_HYSTERESIS>(60);

			/* 3a: Enable RC6 */
			/*
			 * power-gating, special case for Meteor Lake omitted, power-gating for
			 * VCS omitted
			 */
			_mmio.write<Mmio::GEN9_PG_ENABLE::Gen9_render_pg_enable>(1);

			/*
			 * Rc6
			 */
			_mmio.write<Mmio::RC_CTRL1::Target>(4); /* target RC6 */
			Mmio::RC_CTRL0::Ei_hw::set(_ctrl, 1);
			Mmio::RC_CTRL0::Rc6_enable::set(_ctrl, 1);
			Mmio::RC_CTRL0::Hw_control_enable::set(_ctrl, 1);
		}

		void _handle_timer()
		{
			if (!_progress) {
				_enter_rc6();
				return;
			}

			_progress = false;
			_rc6_watchdog.trigger_once(CHECK_INACTIVE);
		}

		void _enter_rc6()
		{
			/* disable HW timers and enter RC6 */
			_mmio.write<Mmio::RC_CTRL0>(0);
			_mmio.write<Mmio::RC_CTRL0::Rc6_enable>(1);
			_mmio.write<Mmio::RC_CTRL1::Target>(4); /* target RC6 */

			_suspended = true;
		}

		void _resume()
		{
			_mmio.write<Mmio::RC_CTRL0>(_ctrl);

			if (_mmio.generation() == 9)
				_mmio.write<Mmio::RC_CTRL1::Target>(0);

			_rc6_watchdog.trigger_once(CHECK_INACTIVE);

			_suspended = false;
		}

	public:

		Rc6(Genode::Env &env, Mmio &mmio)
		: _env(env), _mmio(mmio)
		{
			_rc6_watchdog.sigh(_timer_sigh);
		}

		void clear()
		{
			if (_mmio.generation() >= 9)
				_mmio.write<Mmio::GEN9_PG_ENABLE>(0);

			_mmio.write<Mmio::RC_CTRL0::Rc6_enable>(0);
			_mmio.write<Mmio::RC_CTRL0>(0);
			_mmio.write<Mmio::RP_CTRL>(0);
			_mmio.write_post<Mmio::RC_CTRL1::Target>(0);
		}

		void enable()
		{
			clear();

			if (_mmio.generation() >= 11) {
				_gen11_enable();
			}
			else if (_mmio.generation() >= 9) {
				_gen9_enable();
				if (_pctx_corrupted()) return;
			}
			else return;

			_resume();
		}

		void progress()
		{
			if (_suspended) _resume();
			if (!_progress) _progress = true;
		}
};

#endif /* _RC6_H_ */
