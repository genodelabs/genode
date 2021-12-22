/*
 * \brief  Frame-buffer driver for Freescale's i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \author Stefan Kalkowski
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
#include <drivers/defs/imx53.h>
#include <base/attached_io_mem_dataspace.h>
#include <io_mem_session/connection.h>
#include <legacy/imx53/platform_session/connection.h>
#include <util/mmio.h>
#include <capture_session/connection.h>

/* local includes */
#include <ipu.h>

namespace Framebuffer {
	using namespace Genode;
	using Area = Capture::Area;
	class Driver;
};


class Framebuffer::Driver
{
	private:

		Genode::Env &_env;

		Platform::Connection      _platform;
		Attached_io_mem_dataspace _ipu_mmio;
		Ipu                       _ipu;
		bool                      _disp0;
		size_t                    _width;
		size_t                    _height;

	public:

		enum Resolutions { BYTES_PER_PIXEL  = 4 };

		Driver(Genode::Env &env, Genode::Xml_node config)
		: _env(env),
		  _platform(_env),
		  _ipu_mmio(_env, Imx53::IPU_BASE, Imx53::IPU_SIZE),
		  _ipu((addr_t)_ipu_mmio.local_addr<void>()),
		  _disp0(config.attribute_value<unsigned>("display", 0) == 0),
		  _width(config.attribute_value<unsigned>("width", 800)),
		  _height(config.attribute_value<unsigned>("height", 480)) { }

		bool init(addr_t phys_base)
		{
			/* enable IPU via platform driver */
			_platform.enable(Platform::Session::IPU);
			_ipu.init(_width, _height, _width * BYTES_PER_PIXEL,
			          phys_base, _disp0);
			return true;
		}

		Area screen_size() const { return Area { _width, _height }; }

		Ipu &ipu()  { return _ipu; }
};

#endif /* _DRIVERS__FRAMEBUFFER__SPEC__IMX53__DRIVER_H_ */
