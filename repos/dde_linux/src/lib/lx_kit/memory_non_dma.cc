/*
 * \brief  Lx_kit without DMA-capable memory allocation backend
 * \author Stefan Kalkowski
 * \date   2021-03-25
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <util/touch.h>

/* local includes */
#include <lx_kit/memory.h>

using namespace Genode;

class Non_dma_buffer : Attached_ram_dataspace,
                       public Lx_kit::Mem_allocator::Buffer
{
	public:

		using Attached_ram_dataspace::Attached_ram_dataspace;

		/* emulate idempotent virt-dma mapping */
		size_t dma_addr() const override {
			return (size_t) Attached_ram_dataspace::local_addr<void>(); }

		size_t size() const override {
			return Attached_ram_dataspace::size(); }

		size_t virt_addr() const override {
			return (size_t) Attached_ram_dataspace::local_addr<void>(); }

		Dataspace_capability cap() override {
			return Attached_ram_dataspace::cap(); }
};


Lx_kit::Mem_allocator::Buffer &
Lx_kit::Mem_allocator::alloc_buffer(size_t size)
{
	size = align_addr(size, 12);

	Buffer & buffer = *static_cast<Buffer*>(new (_heap)
		Non_dma_buffer(_env.ram(), _env.rm(), size, _cache_attr));

	/* map eager by touching all pages once */
	for (size_t sz = 0; sz < buffer.size(); sz += 4096) {
		touch_read((unsigned char const volatile*)(buffer.virt_addr() + sz)); }

	_virt_to_dma.insert(buffer.virt_addr(), buffer);
	_dma_to_virt.insert(buffer.dma_addr(),  buffer);
	return buffer;
}
