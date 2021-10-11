/*
 * \brief  GPU resource handling
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2021-07-16
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <region_map/client.h>
#include <rm_session/connection.h>

namespace Igd {
	class Resources;
}

struct Main;

class Igd::Resources : Genode::Noncopyable
{
	public:

		struct Initialization_failed : Genode::Exception { };

	private:

		using Io_mem_connection = Genode::Io_mem_connection;
		using Io_mem_dataspace_capability = Genode::Io_mem_dataspace_capability;
		using Ram_dataspace_capability = Genode::Ram_dataspace_capability;
		using Dataspace_capability = Genode::Dataspace_capability;

		Genode::Env  &_env;
		Genode::Heap &_heap;

		/* timer */
		Timer::Connection         _timer { _env };

		struct Timer_delayer : Genode::Mmio::Delayer
		{
			Timer::Connection &_timer;
			Timer_delayer(Timer::Connection &timer) : _timer(timer) { }

			void usleep(uint64_t us) override { _timer.usleep(us); }

		} _delayer { _timer };

		/* irq callback */
		Main        &_obj;
		void (Main::*_ack_irq) ();

		/* platform session */
		Platform::Connection     _platform { _env };

		Platform::Device_capability _gpu_cap { };
		Platform::Device_capability _host_bridge_cap { };
		Platform::Device_capability _isa_bridge_cap { };
		Genode::Constructible<Platform::Device_client> _gpu_client { };

		/* mmio + ggtt */
		Platform::Device::Resource               _gttmmadr { };
		Io_mem_dataspace_capability              _gttmmadr_ds { };
		Genode::Constructible<Io_mem_connection> _gttmmadr_io { };
		addr_t                                   _gttmmadr_local { 0 };

		Genode::Constructible<Igd::Mmio> _mmio { };

		/* scratch page for ggtt */
		Ram_dataspace_capability _scratch_page_ds {
			_platform.with_upgrade([&] () {
				return _platform.alloc_dma_buffer(PAGE_SIZE, Genode::UNCACHED); }) };

		addr_t _scratch_page {
			Genode::Dataspace_client(_scratch_page_ds).phys_addr() };

		/* aperture */
		enum {
			/* reserved aperture for platform service */
			APERTURE_RESERVED = 64u<<20,
			/* reserved GTT for platform service, GTT entry is 8 byte */
			GTT_RESERVED      = (APERTURE_RESERVED/PAGE_SIZE) * 8,
		};

		Genode::Rm_connection _rm_connection { _env };

		Platform::Device::Resource               _gmadr { };
		Io_mem_dataspace_capability              _gmadr_ds { };
		Genode::Constructible<Io_mem_connection> _gmadr_io { };
		Genode::Region_map_client                _gmadr_rm { _rm_connection.create(APERTURE_RESERVED) };


		/* managed dataspace for local platform service */
		Genode::Constructible<Genode::Region_map_client> _gttmmadr_rm { };

		void _create_gttmmadr_rm()
		{
			using off_t = Genode::off_t;

			size_t const gttm_half_size = gttmmadr_size() / 2;
			/* GTT starts at half of the mmio memory */
			off_t  const gtt_offset = gttm_half_size;

			if (gttm_half_size < GTT_RESERVED) {
				Genode::error("GTTM size too small");
				return;
			}

			_gttmmadr_rm.construct(_rm_connection.create((gttmmadr_size())));

			/* attach actual iomem + reserved */
			_gttmmadr_rm->attach_at(_gttmmadr_ds, 0, gtt_offset);

			/* attach beginning of GTT */
			_gttmmadr_rm->attach_at(_gttmmadr_ds, gtt_offset, GTT_RESERVED, gtt_offset);

			/* attach the rest of the GTT as dummy RAM */
			Genode::Ram_dataspace_capability dummmy_gtt_ds { _env.ram().alloc(PAGE_SIZE) };
			size_t remainder = gttm_half_size - GTT_RESERVED;
			for (off_t offset = gtt_offset + GTT_RESERVED;
			     remainder > 0;
			     offset += PAGE_SIZE, remainder -= PAGE_SIZE) {
				_rm_connection.retry_with_upgrade(Genode::Ram_quota{4096},
				                                  Genode::Cap_quota{8}, [&]() {
					_gttmmadr_rm->attach_at(dummmy_gtt_ds, offset, PAGE_SIZE);
				 });
			}
		}

		/*********
		 ** Pci **
		 *********/

		void  _find_devices()
		{
			using namespace Platform;

			auto _scan_pci = [&] (Platform::Connection    &pci,
			                      Device_capability const &prev,
			                      bool release) {
				Device_capability cap = pci.with_upgrade([&]() {
					return pci.next_device(prev, 0, 0); });

				if (prev.valid() && release) { pci.release_device(prev); }
				return cap;
			};

			Device_capability cap;
			bool release = false;
			while ((cap = _scan_pci(_platform, cap, release)).valid()) {
				Device_client device(cap);

				unsigned char bus = 0xff, dev = 0xff, func = 0xff;
				device.bus_address(&bus, &dev, &func);

				/* host pci bridge */
				if (bus == 0 && dev == 0 && func == 0) {
					_host_bridge_cap = cap;
					release = false;
					continue;
				}

				/* gpu */
				if ((device.class_code() >> 8) == 0x0300) {
					_gpu_cap = cap;
					release  = false;
					continue;
				}

				if (device.class_code() == isa_bridge_class()) {
					_isa_bridge_cap = cap;
					release = false;
					continue;
				}

				release = true;
			}
		}

		bool _mch_enabled()
		{
			using namespace Platform;

			if (!_host_bridge_cap.valid()) { return false; }

			Device_client device(_host_bridge_cap);

			/*
			 * 5th Gen Core Processor datasheet vol 2 p. 48
			 */
			enum { MCHBAR_OFFSET = 0x48, };
			struct MCHBAR : Genode::Register<64>
			{
				struct Mchbaren : Bitfield<0, 1> { };
			};

			MCHBAR::access_t const v = device.config_read(MCHBAR_OFFSET,
			                                              Platform::Device::ACCESS_32BIT);
			return MCHBAR::Mchbaren::get(v);
		}

		template <typename T>
		static Platform::Device::Access_size _access_size(T)
		{
			switch (sizeof(T)) {
				case 1:  return Platform::Device::ACCESS_8BIT;
				case 2:  return Platform::Device::ACCESS_16BIT;
				default: return Platform::Device::ACCESS_32BIT;
			}
		}

		void _enable_pci_bus_master()
		{
			enum {
				PCI_CMD_REG    = 4,
				PCI_BUS_MASTER = 1<<2,
			};

			uint16_t cmd  = config_read<uint16_t>(PCI_CMD_REG);
			         cmd |= PCI_BUS_MASTER;
			config_write(PCI_CMD_REG, cmd);
		}

	public:

		Resources(Genode::Env &env, Genode::Heap &heap,
		          Main &obj, void (Main::*ack_irq) ())
		:
		  _env(env), _heap(heap), _obj(obj), _ack_irq(ack_irq)
		{
			/* initial donation for device pd */
			_platform.upgrade_ram(1024*1024);

			_find_devices();
			if (!_gpu_cap.valid() || !_mch_enabled()) {
				throw Initialization_failed();
			}

			_gpu_client.construct(_gpu_cap);

			_gttmmadr = _gpu_client->resource(0);
			_gttmmadr_io.construct(_env, _gttmmadr.base(), _gttmmadr.size());
			_gttmmadr_ds = _gttmmadr_io->dataspace();

			_gmadr = _gpu_client->resource(2);
			_gmadr_io.construct(_env, _gmadr.base(), _gmadr.size(), true);
			_gmadr_ds = _gmadr_io->dataspace();
			_gmadr_rm.attach_at(_gmadr_ds, 0, APERTURE_RESERVED);

			_enable_pci_bus_master();

			log("Reserved beginning ",
			    Genode::Number_of_bytes(APERTURE_RESERVED),
			    " of aperture for platform service");
		}

		~Resources()
		{
			_platform.release_device(_gpu_cap);
			_platform.release_device(_host_bridge_cap);
		}

		Genode::Rm_connection &rm() { return _rm_connection; }

		addr_t map_gttmmadr()
		{
			if (!_gttmmadr_ds.valid())
				throw Initialization_failed();

			if (_gttmmadr_local) return _gttmmadr_local;

			_gttmmadr_local = (addr_t)(_env.rm().attach(_gttmmadr_ds, _gttmmadr.size()));

			log("Map res:", 0,
			    " base:", Genode::Hex(_gttmmadr.base()),
			    " size:", Genode::Hex(_gttmmadr.size()),
			    " vaddr:", Genode::Hex(_gttmmadr_local));

			return _gttmmadr_local;
		}

		template <typename T>
		T config_read(unsigned int const devfn)
		{
			T val = 0;
			_platform.with_upgrade([&] () {
				val = _gpu_client->config_read(devfn, _access_size(val));
			});

			return val;
		}

		template <typename T>
		void config_write(unsigned int const devfn, T val)
		{
			_platform.with_upgrade([&] () {
				_gpu_client->config_write(devfn, val, _access_size(val));
			});
		}

		void ack_irq() { (_obj.*_ack_irq)(); }

		Genode::Heap      &heap()               { return _heap; }
		Timer::Connection &timer()              { return _timer; };
		addr_t             scratch_page() const { return _scratch_page; }

		Platform::Connection       &platform()        { return _platform; }
		Platform::Device_client    &gpu_client()      { return *_gpu_client; }
		Platform::Device_capability host_bridge_cap() { return _host_bridge_cap; }
		Platform::Device_capability isa_bridge_cap()  { return _isa_bridge_cap; }
		unsigned                    isa_bridge_class() const { return 0x601u << 8; }

		addr_t               gmadr_base() const { return _gmadr.base(); }
		size_t               gmadr_size() const { return _gmadr.size(); }
		Dataspace_capability gmadr_ds()   const { return _gmadr_ds; }

		addr_t gttmmadr_base() const { return _gttmmadr.base(); }
		size_t gttmmadr_size() const { return _gttmmadr.size(); }

		size_t gmadr_platform_size()    const { return APERTURE_RESERVED; }
		size_t gttmmadr_platform_size() const { return GTT_RESERVED; }

		Io_mem_dataspace_capability gttmmadr_platform_ds()
		{
			using namespace Genode;

			if (!_gttmmadr_rm.constructed())
				_create_gttmmadr_rm();

			if (!_gttmmadr_rm.constructed())
				return { };

			return static_cap_cast<Io_mem_dataspace>(_gttmmadr_rm->dataspace());
		}

		Io_mem_dataspace_capability gmadr_platform_ds()
		{
			using namespace Genode;
			return static_cap_cast<Io_mem_dataspace>(_gmadr_rm.dataspace());
		}

		void gtt_platform_reset()
		{
			addr_t const base = map_gttmmadr() + (gttmmadr_size() / 2);
			Igd::Ggtt(mmio(), base, gttmmadr_platform_size(), 0, scratch_page(), 0);
		}

		Igd::Mmio &mmio()
		{
			if (!_mmio.constructed())
				_mmio.construct(_delayer, map_gttmmadr());

			return *_mmio;
		}
};

