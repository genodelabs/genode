/*
 * \brief  Driver for i.MX53 specific platform devices (clocks, power, etc.)
 * \author Stefan Kalkowski <stefan.kalkowski@genode-labs.com>
 * \date   2013-04-29
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/sleep.h>
#include <base/rpc_server.h>
#include <cap_session/connection.h>
#include <root/component.h>
#include <platform_session/platform_session.h>

#include <ccm.h>
#include <iim.h>
#include <iomux.h>
#include <src.h>


namespace Platform {
	class Session_component;
	class Root;
}


class Platform::Session_component : public Genode::Rpc_object<Platform::Session>
{
	private:

		Iim   &_iim;   /* IC identification module */
		Iomux &_iomux; /* I/O multiplexer device   */
		Ccm   &_ccm;   /* clock control module     */
		Src   &_src;   /* system reset controller  */

	public:

		/**
		 * Constructor
		 */
		Session_component(Iim &iim, Iomux &iomux, Ccm &ccm, Src &src)
		: _iim(iim), _iomux(iomux), _ccm(ccm), _src(src) {}


		/**********************************
		 **  Platform session interface  **
		 **********************************/

		void enable(Device dev)
		{
			switch (dev) {
			case Session::IPU:
				_src.reset_ipu();
				_ccm.ipu_clk_enable();
				_iomux.ipu_enable();
				break;
			case Session::I2C_2:
				_ccm.i2c_2_enable();
				_iomux.i2c_2_enable();
				break;
			case Session::I2C_3:
				_ccm.i2c_3_enable();
				_iomux.i2c_3_enable();
				break;
			case Session::BUTTONS:
				_iomux.buttons_enable();
				break;
			case Session::PWM:
				_iomux.pwm_enable();
				break;
			default:
				Genode::warning("invalid device");
			};
		}

		void disable(Device dev)
		{
			switch (dev) {
			case Session::IPU:
				_ccm.ipu_clk_disable();
				break;
			default:
				Genode::warning("invalid device");
			};
		}

		void clock_rate(Device dev, unsigned long rate)
		{
			switch (dev) {
			default:
				Genode::warning("invalid device");
			};
		}

		Board_revision revision()
		{
			switch (_iim.revision()) {
			case QSB: return QSB;
			case SMD: return SMD;
			};
			return UNKNOWN;
		}
};


class Platform::Root : public Genode::Root_component<Platform::Session_component>
{
	private:

		Iim   _iim;
		Iomux _iomux;
		Ccm   _ccm;
		Src   _src;

	protected:

		Session_component *_create_session(const char *args) {
			return new (md_alloc()) Session_component(_iim, _iomux, _ccm, _src); }

	public:

		Root(Genode::Rpc_entrypoint *session_ep,
		     Genode::Allocator *md_alloc)
		: Genode::Root_component<Session_component>(session_ep, md_alloc) { }
};


int main(int, char **)
{
	using namespace Genode;

	Genode::log("--- i.MX53 platform driver ---");

	static Cap_connection cap;
	static Rpc_entrypoint ep(&cap, 4096, "imx53_plat_ep");
	static Platform::Root plat_root(&ep, env()->heap());
	env()->parent()->announce(ep.manage(&plat_root));

	sleep_forever();
	return 0;
}
