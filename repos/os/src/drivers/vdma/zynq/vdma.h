/*
 * \brief  VDMA Driver for Zynq
 * \author Mark Albers
 * \date   2015-04-13
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _VDMA_H_
#define _VDMA_H_

#include <os/attached_io_mem_dataspace.h>
#include <util/mmio.h>

namespace Vdma {
	using namespace Genode;
    class Zynq_Vdma;
}

struct Vdma::Zynq_Vdma : Attached_io_mem_dataspace, Mmio
{
    Zynq_Vdma(Genode::addr_t const mmio_base, Genode::size_t const mmio_size) :
		Genode::Attached_io_mem_dataspace(mmio_base, mmio_size),
	  	Genode::Mmio((Genode::addr_t)local_addr<void>())
    { }

    ~Zynq_Vdma()
    { }

	/*
	 * Registers
	 */

    struct MM2S_VDMACR : Register<0x00, 32>
    {
        struct Repeat_En : Bitfield<15,1> {};
        struct Err_IrqEn : Bitfield<14,1> {};
        struct RdPntrNum : Bitfield<8,4> {};
        struct GenlockSrc : Bitfield<7,1> {};
        struct GenlockEn : Bitfield<3,1> {};
        struct Reset : Bitfield<2,1> {};
        struct Circular_Park : Bitfield<1,1> {};
        struct RS : Bitfield<0,1> {};
    };

    struct MM2S_VDMASR : Register<0x04, 32>
    {
        struct Err_Irq : Bitfield<14,1> {};
        struct SOFEarlyErr : Bitfield<7,1> {};
        struct VDMADecErr : Bitfield<6,1> {};
        struct VDMASlvErr : Bitfield<5,1> {};
        struct VDMAIntErr : Bitfield<4,1> {};
        struct Halted : Bitfield<0,1> {};
    };

    struct MM2S_REG_INDEX : Register<0x14, 32>
    {
        struct MM2S_Reg_Index : Bitfield<0,1> {};
    };

    struct PARK_PTR_REG : Register<0x28, 32>
    {
        struct WrFrmStore : Bitfield<24,5> {};
        struct RdFrmStore : Bitfield<16,5> {};
        struct WrFrmPtrRef : Bitfield<8,5> {};
        struct RdFrmPtrRef : Bitfield<0,5> {};
    };

    struct VDMA_VERSION : Register<0x2c, 32>
    {
        struct Major_Version : Bitfield<28,4> {};
        struct Minor_Version : Bitfield<20,8> {};
        struct Revision : Bitfield<16,4> {};
        struct Xilinx_Internal : Bitfield<0,16> {};
    };

    struct S2MM_VDMACR : Register<0x30, 32>
    {
        struct Repeat_En : Bitfield<15,1> {};
        struct Err_IrqEn : Bitfield<14,1> {};
        struct WrPntrNum : Bitfield<8,4> {};
        struct GenlockSrc : Bitfield<7,1> {};
        struct GenlockEn : Bitfield<3,1> {};
        struct Reset : Bitfield<2,1> {};
    };

    struct S2MM_VDMASR : Register<0x34, 32>
    {
        struct EOLLateErr : Bitfield<15,1> {};
        struct Err_Irq : Bitfield<14,1> {};
        struct SOFLateErr : Bitfield<11,1> {};
        struct EOLEarlyErr : Bitfield<8,1> {};
        struct SOFEarlyErr : Bitfield<7,1> {};
        struct VDMADecErr : Bitfield<6,1> {};
        struct DMASlvErr : Bitfield<5,1> {};
        struct DMAIntErr : Bitfield<4,1> {};
        struct Halted : Bitfield<0,1> {};
    };

    struct S2MM_REG_INDEX : Register<0x44, 32>
    {
        struct S2MM_Reg_Index : Bitfield<0,1> {};
    };

    struct MM2S_VSIZE : Register<0x50, 32>
    {
        struct Vertical_Size : Bitfield<0,13> {};
    };

    struct MM2S_HSIZE : Register<0x54, 32>
    {
        struct Horizontal_Size : Bitfield<0,16> {};
    };

    struct MM2S_FRMDLY_STRIDE : Register<0x58, 32>
    {
        struct Frame_Delay : Bitfield<24,5> {};
        struct Stride : Bitfield<0,16> {};
    };

    struct MM2S_START_ADDRESS : Register<0x5c, 32> {};

    struct S2MM_VSIZE : Register<0xa0, 32>
    {
        struct Vertical_Size : Bitfield<0,13> {};
    };

    struct S2MM_HSIZE : Register<0xa4, 32>
    {
        struct Horizontal_Size : Bitfield<0,16> {};
    };

    struct S2MM_FRMDLY_STRIDE : Register<0xa8, 32>
    {
        struct Frame_Delay : Bitfield<24,5> {};
        struct Stride : Bitfield<0,16> {};
    };

    struct S2MM_START_ADDRESS : Register<0xac, 32> {};

};

#endif // _VDMA_H_
