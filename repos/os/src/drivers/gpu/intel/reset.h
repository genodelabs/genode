/*
 * \brief  Render engine reset  based on the Linux driver
 * \author Sebastian Sumpf
 * \date   2023-06-05
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RESET_H_
#define _RESET_H_

#include "mmio.h"
#include "workarounds.h"

namespace Igd {
	class Reset;
}


class Igd::Reset
{
	private:

		void _stop_engine_cs(Igd::Mmio &mmio)
		{
			/* write stop bit to render mode */
			mmio.write<Mmio::CS_MI_MODE_CTRL::Stop_rings>(1);

			/*
			 * Wa_22011802037 : GEN11, GNE12, Prior to doing a reset, ensure CS is
			 * stopped, set ring stop bit and prefetch disable bit to halt CS
			 */
			if (mmio.generation() == 11 || mmio.generation() == 12) {
				Mmio::GFX_MODE::access_t v = 0;
				using G = Mmio::GFX_MODE;
				v = G::set<G::Gen12_prefetch_disable>(v, 1);
				mmio.write<Mmio::GFX_MODE>(v);
			}

			try {
				mmio.wait_for(Mmio::Attempts(10), Mmio::Microseconds(100'000), mmio.delayer(),
				               Mmio::CS_MI_MODE_CTRL::Rings_idle::Equal(1));
			} catch(Mmio::Polling_timeout) {
				Genode::warning("stop engine cs timeout");
			}

			/* read to let GPU writes be flushed to memory */
			mmio.read<Mmio::CS_MI_MODE_CTRL>();
		}

		/* not documented
		 * Wa_22011802037: gen11/gen12: In addition to stopping the cs, we need
		 * to wait for any pending mi force wakeups
		 * MSG_IDLE_CS 0x8000 force wake
		 */
		void _wait_for_pending_force_wakeups(Igd::Mmio &mmio)
		{
			if (mmio.generation() < 11 && mmio.generation() > 12) return;

			unsigned fw_status = mmio.read<Mmio::MSG_IDLE_CS::Pending_status>();
			unsigned fw_mask   = mmio.read<Mmio::MSG_IDLE_CS::Pending_mask>();

			mmio.delayer().usleep(1);

			for (unsigned i = 0; i < 10; i++) {
				unsigned status = mmio.read<Mmio::GEN9_PWRGT_DOMAIN_STATUS>() & fw_mask;

				mmio.delayer().usleep(1);

				if (status == fw_status) return;

				mmio.delayer().usleep(50000);
			}

			mmio.delayer().usleep(1);
			Genode::warning("wait pending force wakeup timeout");
		}

		void _ready_for_reset(Igd::Mmio &mmio)
		{
			if (mmio.read<Mmio::CS_RESET_CTRL::Catastrophic_error>()) {
				/* For catastrophic errors, ready-for-reset sequence
				 * needs to be bypassed: HAS#396813
				 */
				try {
					mmio.wait_for(Mmio::Attempts(7), Mmio::Microseconds(100'000), mmio.delayer(),
					               Mmio::CS_RESET_CTRL::Catastrophic_error::Equal(0));
				} catch (Mmio::Polling_timeout) {
					Genode::warning("catastrophic error reset not cleared");
				}
				return;
			}

			if (mmio.read<Mmio::CS_RESET_CTRL::Ready_for_reset>()) return;

			Mmio::CS_RESET_CTRL::access_t request = 0;
			Mmio::CS_RESET_CTRL::Mask_bits::set(request, 1);
			Mmio::CS_RESET_CTRL::Request_reset::set(request, 1);
			mmio.write_post<Mmio::CS_RESET_CTRL>(request);
			try {
				mmio.wait_for(Mmio::Attempts(7), Mmio::Microseconds(100'000), mmio.delayer(),
				               Mmio::CS_RESET_CTRL::Ready_for_reset::Equal(1));
			} catch (Mmio::Polling_timeout) {
				Genode::warning("not ready for reset");
			}
		}

		void _unready_for_reset(Igd::Mmio &mmio)
		{
			Mmio::CS_RESET_CTRL::access_t request = 0;
			Mmio::CS_RESET_CTRL::Mask_bits::set(request, 1);
			Mmio::CS_RESET_CTRL::Request_reset::set(request, 0);
			mmio.write_post<Mmio::CS_RESET_CTRL>(request);
		}

		void _reset_hw(Igd::Mmio &mmio)
		{
			/* full sw reset */
			mmio.write<Mmio::GDRST::Graphics_full_soft_reset_ctl>(1);
			try {
				/* do NOT attempt more than 2 times */
				mmio.wait_for(Mmio::Attempts(2), Mmio::Microseconds(200'000), mmio.delayer(),
				               Mmio::GDRST::Graphics_full_soft_reset_ctl::Equal(0));
			} catch (Mmio::Polling_timeout) {
				Genode::error("resetting device failed");
			}

			/* some devices still show volatile state */
			mmio.delayer().usleep(50);
		}

		void _init_swizzling(Igd::Mmio &mmio)
		{
			mmio.write<Mmio::DISP_ARB_CTL::DISP_TILE_SURFACE_SWIZZLING>(1);
			mmio.write<Mmio::TILECTL::SWZCTL>(1);

			if (mmio.generation() == 8)
				mmio.write<Mmio::GAMTARBMODE::Arbiter_mode_control_1>(1);
		}

	public:

		Reset() { }

		void execute(Igd::Mmio &mmio)
		{
			_stop_engine_cs(mmio);
			_wait_for_pending_force_wakeups(mmio);
			_ready_for_reset(mmio);

			_reset_hw(mmio);

			_unready_for_reset(mmio);

			if (mmio.generation() < 9)
				mmio.write<Mmio::HSW_IDICR::Idi_hash_mask>(0xf);

			apply_workarounds(mmio, mmio.generation());
			_init_swizzling(mmio);
			mmio.enable_execlist();
		}
};

#endif /* _RESET_H_ */
