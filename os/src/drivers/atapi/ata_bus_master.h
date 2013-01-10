/*
 * \brief  I/O interface to the IDE bus master
 * \author Sebastian Sumpf <Sebastian.Sumpf@genode-labs.com>
 * \date   2010-07-15
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _ATA_PCI_H_
#define _ATA_PCI_H_

namespace Genode {
	class Io_port_connection;
}

namespace Ata {

	class Bus_master {

		enum {
			PCI_CFG_BMIBA_OFF  = 0x20, /* offset in PCI config space */
			CLASS_MASS_STORAGE = 0x10000,
			SUBCLASS_IDE       = 0x0100,
			CLASS_MASK         = 0xffff00,

			/* These bits of the programming interface part within the class code
			 * define if the corresponding channel operates on standard legacy ports
			 * (bits are cleared) or if the corresponding ports in base register 0-1
			 * and 2-3 for channel 2 must be used as I/O bases (bits are set) */
			PI_CH1_LEGACY      = 0x1,
			PI_CH2_LEGACY      = 0x4,
		};

		/**
		 * Bus master interface base address
		 */
		unsigned _bmiba;

		bool                        _port_io;
		bool                        _secondary;
		unsigned long               _prd_virt;
		unsigned long               _prd_phys;
		Genode::Io_port_connection *_pio;

		public:

			Bus_master(bool secondary);
			~Bus_master();

			unsigned char read_cmd();
			unsigned char read_status();

			void write_cmd(unsigned char val);
			void write_status(unsigned char val);
			void write_prd(unsigned long val);

			void set_prd(unsigned long virt, unsigned long phys)
			{
				_prd_virt = virt;
				_prd_phys  = phys;
			}

			/**
			 * Scan PCI Bus for IDE devices and set BMBIA
			 */
			bool scan_pci();

	};
}
#endif /* _ATA_PCI_H_ */
