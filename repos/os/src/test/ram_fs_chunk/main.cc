/*
 * \brief  Unit test for RAM fs chunk data structure
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-04-19
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/heap.h>
#include <base/component.h>
#include <ram_fs/chunk.h>

using namespace File_system;
using namespace Genode;

using Chunk_level_3 = Chunk<2>;
using Chunk_level_2 = Chunk_index<3, Chunk_level_3>;
using Chunk_level_1 = Chunk_index<4, Chunk_level_2>;

struct Chunk_level_0 : Chunk_index<5, Chunk_level_1>
{
	Chunk_level_0(Allocator &alloc, Seek off) : Chunk_index(alloc, off) { }

	void print(Output &out) const
	{
		static char read_buf[Chunk_level_0::SIZE];
		if (used_size() > Chunk_level_0::SIZE) {
			throw Index_out_of_range(); }

		read(Byte_range_ptr(read_buf, (size_t)used_size()), Seek { 0 });
		Genode::print(out, "content (size=", used_size(), "): ");
		Genode::print(out, "\"");
		for (unsigned i = 0; i < used_size(); i++) {
			char const c = read_buf[i];
			if (c) {
				Genode::print(out, Char(c)); }
			else {
				Genode::print(out, "."); }
		}
		Genode::print(out, "\"");
	}
};

struct Allocator_tracer : Allocator
{
	struct Alloc
	{
		using Id = Id_space<Alloc>::Id;

		Id_space<Alloc>::Element id_space_elem;
		size_t                   size;

		Alloc(Id_space<Alloc> &space, Id id, size_t size)
		: id_space_elem(*this, space, id), size(size) { }
	};

	Id_space<Alloc>  allocs { };
	size_t           sum { 0 };
	Allocator       &wrapped;

	Allocator_tracer(Allocator &wrapped) : wrapped(wrapped) { }

	Alloc_result try_alloc(size_t size) override
	{
		return wrapped.try_alloc(size).convert<Alloc_result>(

			[&] (void *ptr) {
				sum += size;
				new (wrapped) Alloc(allocs, Alloc::Id { (addr_t)ptr }, size);
				return ptr;
			},

			[&] (Allocator::Alloc_error error) {
				return error;
			}
		);
	}

	void free(void *addr, size_t size) override
	{
		allocs.apply<Alloc>(Alloc::Id { (addr_t)addr }, [&] (Alloc &alloc) {
			sum -= alloc.size;
			destroy(wrapped, &alloc);
			wrapped.free(addr, size);
		});
	}

	size_t overhead(size_t size) const override { return wrapped.overhead(size); }
	bool   need_size_for_free()  const override { return wrapped.need_size_for_free(); }
};

struct Main
{
	using Seek = Chunk_base::Seek;

	Env              &env;
	Heap              heap  { env.ram(), env.rm() };
	Allocator_tracer  alloc { heap };

	Main(Env &env) : env(env)
	{
		log("--- RAM filesystem chunk test ---");
		log("chunk sizes");
		log("  level 0: payload=", (int)Chunk_level_0::SIZE, " sizeof=", sizeof(Chunk_level_0));
		log("  level 1: payload=", (int)Chunk_level_1::SIZE, " sizeof=", sizeof(Chunk_level_1));
		log("  level 2: payload=", (int)Chunk_level_2::SIZE, " sizeof=", sizeof(Chunk_level_2));
		log("  level 3: payload=", (int)Chunk_level_3::SIZE, " sizeof=", sizeof(Chunk_level_3));
		{
			Chunk_level_0 chunk(alloc, Seek { 0 });
			write(chunk, "five-o-one", Seek { 0 });

			/* overwrite part of the file */
			write(chunk, "five", Seek { 7 });

			/* write to position beyond current file length */
			write(chunk, "Nuance", Seek { 17 });
			write(chunk, "YM-2149", Seek { 35 });

			truncate(chunk, Seek { 30 });
			for (unsigned i = 29; i > 0; i--)
				truncate(chunk, Seek { i });
		}
		log("allocator: sum=", alloc.sum);
		log("--- RAM filesystem chunk test finished ---");
	}

	void write(Chunk_level_0 &chunk, char const *str, Seek seek)
	{
		chunk.write(Const_byte_range_ptr(str, strlen(str)), seek);

		log("write \"", str, "\" at offset ", seek.value, " -> ", chunk);
	}

	void truncate(Chunk_level_0 &chunk, Seek size)
	{
		chunk.truncate(size);
		log("trunc(", size.value, ") -> ", chunk);
	}
};

void Component::construct(Env &env) { struct Main main(env); }
