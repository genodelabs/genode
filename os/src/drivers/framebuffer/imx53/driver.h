/*
 * \brief  Frame-buffer driver for Freescale's i.MX53
 * \author Nikolay Golikov <nik@ksyslabs.org>
 * \date   2012-06-21
 */

/* Genode includes */
#include <drivers/board_base.h>
#include <os/attached_io_mem_dataspace.h>
#include <io_mem_session/connection.h>
#include <gpio_session/connection.h>
#include <util/mmio.h>

/* local includes */
#include <ipu.h>
#include <src.h>
#include <ccm.h>


namespace Framebuffer {
	using namespace Genode;
	class Driver;
};


class Framebuffer::Driver
{
	private:

		/* Clocks control module */
		Attached_io_mem_dataspace _ccm_mmio;
		Ccm                       _ccm;

		/* System reset controller registers */
		Attached_io_mem_dataspace _src_mmio;
		Src                       _src;

		/* Image processing unit memory */
		Attached_io_mem_dataspace _ipu_mmio;
		Ipu                       _ipu;

		Gpio::Connection          _gpio;

	public:

		enum {
			REFRESH          = 60,
			WIDTH            = 800,
			HEIGHT           = 480,
			PIX_CLK          = 29850,
			ROUND_PIX_CLK    = 38000,
			LEFT_MARGIN      = 89,
			RIGHT_MARGIN     = 104,
			UPPER_MARGIN     = 10,
			LOWER_MARGIN     = 10,
			VSYNC_LEN        = 10,
			HSYNC_LEN        = 10,
			BYTES_PER_PIXEL  = 2,
			FRAMEBUFFER_SIZE = WIDTH * HEIGHT * BYTES_PER_PIXEL,

			LCD_BL_GPIO      = 88,
			LCD_CONT_GPIO    = 1,
		};


		Driver()
		: _ccm_mmio(Board_base::CCM_BASE, Board_base::CCM_SIZE),
		  _ccm((addr_t)_ccm_mmio.local_addr<void>()),
		  _src_mmio(Board_base::SRC_BASE, Board_base::SRC_SIZE),
		  _src((addr_t)_src_mmio.local_addr<void>()),
		  _ipu_mmio(Board_base::IPU_BASE, Board_base::IPU_SIZE),
		  _ipu((addr_t)_ipu_mmio.local_addr<void>()) { }

		bool init(addr_t phys_base)
		{
			/* reset ipu over src */
			_src.write<Src::Ctrl_reg::Ipu_rst>(1);

			_ccm.ipu_clk_enable();

			_ipu.init(WIDTH, HEIGHT, WIDTH * BYTES_PER_PIXEL, phys_base);

			/* Turn on lcd power */
			_gpio.direction_output(LCD_BL_GPIO, true);
			_gpio.direction_output(LCD_CONT_GPIO, true);
			return true;
		}

};

