/*
 * \brief  RAM framebuffer driver for Qemu
 * \author Sebastian Sumpf
 * \date   2021-04-15
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <base/log.h>
#include <capture_session/connection.h>
#include <platform_session/device.h>
#include <timer_session/connection.h>
#include <util/endian.h>
#include <util/mmio.h>

using namespace Genode;

class Main
{
	private:

		enum {
			SCR_WIDTH  = 1024u,
			SCR_HEIGHT = 768u,
			SCR_STRIDE = SCR_WIDTH * 4,
		};

		/*****************************
		 ** Qemu firmware interface **
		 *****************************/

		struct Fw : Mmio
		{
			template <typename T>
			struct Data     : Register<0x0, sizeof(T) * 8> { };
			struct Selector : Register<0x8, 16> { };
			struct Dma      : Register<0x10, 64> { };

			Fw(addr_t const base)
			:
			  Mmio(base) { }
		};

		Env &_env;

		/************************
		 ** Genode integration **
		 ************************/

		using Type = Platform::Device::Type;

		Platform::Connection   _platform { _env };
		Platform::Device       _fw_dev { _platform, Type { "qemu,fw-cfg-mmio" } };
		Platform::Device::Mmio _fw_mem { _fw_dev };
		Fw                     _fw { (addr_t)_fw_mem.local_addr<addr_t>() };

		Ram_dataspace_capability _fb_ds_cap  {
			_platform.alloc_dma_buffer(SCR_HEIGHT * SCR_STRIDE, UNCACHED) };
		Attached_dataspace       _fb_ds      { _env.rm(), _fb_ds_cap };

		Ram_dataspace_capability _config_ds_cap  {
			_platform.alloc_dma_buffer(0x1000, UNCACHED) };
		Attached_dataspace       _config_ds      { _env.rm(), _config_ds_cap };

		Capture::Area const _size { SCR_WIDTH, SCR_HEIGHT };
		Capture::Connection _capture { _env };
		Capture::Connection::Screen _captured_screen { _capture, _env.rm(), _size };

		Timer::Connection _timer { _env };

		Signal_handler<Main> _timer_handler { _env.ep(), *this, &Main::_handle_timer };

		void _handle_timer()
		{
			using Pixel = Capture::Pixel;

			Surface<Pixel> surface(_fb_ds.local_addr<Pixel>(), _size);

			_captured_screen.apply_to_surface(surface);
		}

		/* device selector */
		void _fw_selector(uint16_t key) {
			_fw.write<Fw::Selector>(host_to_big_endian(key)); }

		/* read data for selected key */
		template <typename T> T _fw_data() {
			return host_to_big_endian(_fw.read<Fw::Data<T>>()); }

		/* DMA control structure */
		struct Fw_dma_config : Genode::Mmio
		{
			struct Control : Register<0x0, 32> { };
			struct Length  : Register<0x4, 32> { };
			struct Address : Register<0x8, 64> { };

			Fw_dma_config(addr_t const base)
			:
			  Mmio(base)
			{
				/* set write bit */
				write<Control>(host_to_big_endian(1u << 4));
			}
		};

		/* file structure for directory traversal (key=0x19) */
		struct Fw_config_file
		{
			uint32_t size;
			uint16_t key;
			uint16_t reserved;
			char name[56];
		};

		/* ramfb configuration */
		struct Ram_fb_config : Genode::Mmio
		{
			struct Address    : Register<0x0, 64> { };
			struct Drm_format : Register<0x8, 32> { };
			struct Width      : Register<0x10, 32> { };
			struct Height     : Register<0x14, 32> { };
			struct Stride     : Register<0x18, 32> { };

			Ram_fb_config(addr_t const base)
			:
			  Mmio(base)
			  {
					enum {
						DRM_FORMAT_ARGB8888 = 0x34325241,
					};
					/* RGBA32 */
					write<Drm_format>(host_to_big_endian(DRM_FORMAT_ARGB8888));
					write<Stride>(host_to_big_endian(SCR_STRIDE));
				}
		};

		Fw_config_file _find_ramfb()
		{
			/* file directory */
			_fw_selector(0x19);
			uint32_t count = _fw_data<uint32_t>();

			Fw_config_file file { };
			for (unsigned i = 0; i < count; i++) {

				file.size     = _fw_data<uint32_t>();
				file.key      = _fw_data<uint16_t>();
				file.reserved = _fw_data<uint16_t>();

				for (unsigned j = 0; j < 56; j++)
					file.name[j] = _fw_data<char>();

				if (Genode::strcmp(file.name, "etc/ramfb") == 0) {
					log("RAM FB found with key ", file.key);
					return file;
				}
			}

			error("'etc/ramfb' not found, try the '-device ramfb' option with Qemu");
			throw -1;
		}

		void _setup_framebuffer(Fw_config_file const &file)
		{
			enum { FW_OFFSET = 28 };

			_fw_selector(file.key);

			addr_t config_addr = (addr_t)_config_ds.local_addr<addr_t>();
			addr_t config_phys = (addr_t)_platform.dma_addr(_config_ds_cap);
			addr_t fb_phys     = (addr_t)_platform.dma_addr(_fb_ds_cap);

			Ram_fb_config config { config_addr };
			config.write<Ram_fb_config::Address>(host_to_big_endian(fb_phys));
			config.write<Ram_fb_config::Width>(host_to_big_endian(SCR_WIDTH));
			config.write<Ram_fb_config::Height>(host_to_big_endian(SCR_HEIGHT));

			Fw_dma_config fw_dma { config_addr + FW_OFFSET};
			fw_dma.write<Fw_dma_config::Length>(host_to_big_endian(file.size));
			fw_dma.write<Fw_dma_config::Address>(host_to_big_endian(config_phys));

			_fw.write<Fw::Dma>(host_to_big_endian(config_phys + FW_OFFSET));
		}

	public:

		Main(Env &env)
		:
			_env(env)
		{
			_setup_framebuffer(_find_ramfb());

			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(20*1000);
		}
};

void Component::construct(Genode::Env &env)
{
	log ("--- Qemu Ramfb driver --");
	static Main main(env);
};
