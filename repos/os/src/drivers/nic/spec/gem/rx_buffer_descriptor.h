/*
 * \brief  Base EMAC driver for the Xilinx EMAC PS used on Zynq devices
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__RX_BUFFER_DESCRIPTOR_H_
#define _INCLUDE__DRIVERS__NIC__GEM__RX_BUFFER_DESCRIPTOR_H_

#include "buffer_descriptor.h"

using namespace Genode;


class Rx_buffer_descriptor : public Buffer_descriptor
{
	private:
		struct Addr : Register<0x00, 32> {
			struct Addr31to2 : Bitfield<2, 28> {};
			struct Wrap : Bitfield<1, 1> {};
			struct Package_available : Bitfield<0, 1> {};
		};
		struct Status : Register<0x04, 32> {
			struct Length : Bitfield<0, 13> {};
			struct Start_of_frame : Bitfield<14, 1> {};
			struct End_of_frame : Bitfield<15, 1> {};
		};

		enum { BUFFER_COUNT = 16 };


		/**
		 * @brief _set_buffer_processed resets the available flag
		 * So the DMA controller can use this buffer for an received package.
		 * The buffer index will be incremented, too.
		 * So the right sequenz of packages will be keeped.
		 */
		void _set_package_processed()
		{
			/* reset package available for new package */
			_current_descriptor().addr &= ~Addr::Package_available::bits(1);
			/* use next buffer descriptor for next package */
			_increment_descriptor_index();
		}


	public:
		Rx_buffer_descriptor() : Buffer_descriptor(BUFFER_COUNT)
		{
			/*
			 * mark the last buffer descriptor
			 * so the dma will start at the beginning again
			 */
			_descriptors[BUFFER_COUNT-1].addr |= Addr::Wrap::bits(1);
		}


		bool package_available()
		{
			for (unsigned int i=0; i<BUFFER_COUNT; i++) {
				const bool available = Addr::Package_available::get(_current_descriptor().addr);
				if (available) {
					return true;
				}

				_increment_descriptor_index();
			}

			return false;
		}


		size_t package_length()
		{
			if (!package_available())
				return 0;

			return Status::Length::get(_current_descriptor().status);
		}


		size_t get_package(char* const package, const size_t max_length)
		{
			if (!package_available())
				return 0;

			const Status::access_t status = _current_descriptor().status;
			if (!Status::Start_of_frame::get(status) || !Status::End_of_frame::get(status)) {
				warning("Package splitted over more than one descriptor. Package ignored!");

				_set_package_processed();
				return 0;
			}

			const size_t length = Status::Length::get(status);
			if (length > max_length) {
				warning("Buffer for received package to small. Package ignored!");

				_set_package_processed();
				return 0;
			}

			const char* const src_buffer = _current_buffer();
			memcpy(package, src_buffer, length);

			_set_package_processed();


			return length;
		}

		void show_mem_diffs()
		{
			static unsigned int old_data[0x1F];

			log("Rx buffer:");
			const unsigned int* const cur_data = local_addr<unsigned int>();
			for (unsigned i=0; i<sizeof(old_data)/sizeof(old_data[0]); i++) {
				if (cur_data[i] != old_data[i]) {
					log(i*4, ": ", Hex(old_data[i]), " -> ", Hex(cur_data[i]));
				}
			}
			memcpy(old_data, cur_data, sizeof(old_data));
		}
};

#endif /* _INCLUDE__DRIVERS__NIC__GEM__RX_BUFFER_DESCRIPTOR_H_ */
