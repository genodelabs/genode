/*
 * \brief  Frame-buffer driver for Freescale's i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-06-21
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVERS__FRAMEBUFFER__SPEC__IMX53__DRIVER_H_
#define _DRIVERS__FRAMEBUFFER__SPEC__IMX53__DRIVER_H_

/* Genode includes */
#include <drivers/board_base.h>
#include <base/attached_io_mem_dataspace.h>
#include <io_mem_session/connection.h>
#include <gpio_session/connection.h>
#include <platform_session/connection.h>
#include <util/mmio.h>

/* local includes */
#include <ipu.h>
#include <pwm.h>


namespace Framebuffer {
	using namespace Genode;
	class Driver;
};


class Framebuffer::Driver
{
	private:

		Genode::Env &_env;

		Platform::Connection              _platform;
		Attached_io_mem_dataspace         _ipu_mmio;
		Ipu                               _ipu;
		Attached_io_mem_dataspace         _pwm_mmio;
		Pwm                               _pwm;
		Platform::Session::Board_revision _board;
		size_t                            _width;
		size_t                            _height;

	public:

		enum Resolutions {
			QSB_WIDTH        = 800,
			QSB_HEIGHT       = 480,
			SMD_WIDTH        = 1024,
			SMD_HEIGHT       = 768,
			BYTES_PER_PIXEL  = 2,
		};

		enum Gpio_pins {
			LCD_BL_GPIO      = 88,
			LCD_CONT_GPIO    = 1,
		};

		Driver(Genode::Env &env)
		: _env(env),
		  _platform(_env),
		  _ipu_mmio(_env, Board_base::IPU_BASE, Board_base::IPU_SIZE),
		  _ipu((addr_t)_ipu_mmio.local_addr<void>()),
		  _pwm_mmio(_env, Board_base::PWM2_BASE, Board_base::PWM2_SIZE),
		  _pwm((addr_t)_pwm_mmio.local_addr<void>()),
		  _board(_platform.revision()),
		  _width(_board == Platform::Session::QSB ? QSB_WIDTH : SMD_WIDTH),
		  _height(_board == Platform::Session::QSB ? QSB_HEIGHT : SMD_HEIGHT) { }

		bool init(addr_t phys_base)
		{
			/* enable IPU via platform driver */
			_platform.enable(Platform::Session::IPU);

			switch (_board) {
			case Platform::Session::QSB:
				{
				_ipu.init(_width, _height, _width * BYTES_PER_PIXEL,
				          phys_base, true);

				/* turn display on */
				Gpio::Connection gpio_bl(_env, LCD_BL_GPIO);
				gpio_bl.direction(Gpio::Session::OUT);
				gpio_bl.write(true);
				Gpio::Connection gpio_ct(_env, LCD_CONT_GPIO);
				gpio_ct.direction(Gpio::Session::OUT);
				gpio_ct.write(true);
				break;
				}
			case Platform::Session::SMD:
				_ipu.init(_width, _height, _width * BYTES_PER_PIXEL,
				          phys_base, false);
				_platform.enable(Platform::Session::PWM);
				_pwm.enable_display();
				break;
			default:
				error("unknown board revision!");
				return false;
			}
			return true;
		}

		Mode mode() { return Mode(_width, _height, Mode::RGB565); }
		Ipu &ipu()  { return _ipu; }
};

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__IMX53__DRIVER_H_ */
