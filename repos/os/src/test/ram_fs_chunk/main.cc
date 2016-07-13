/*
 * \brief  Unit test for RAM fs chunk data structure
 * \author Norman Feske
 * \date   2012-04-19
 */

/* Genode includes */
#include <base/env.h>
#include <base/log.h>
#include <ram_fs/chunk.h>

namespace File_system {
	typedef Chunk<2>                      Chunk_level_3;
	typedef Chunk_index<3, Chunk_level_3> Chunk_level_2;
	typedef Chunk_index<4, Chunk_level_2> Chunk_level_1;
	typedef Chunk_index<5, Chunk_level_1> Chunk_level_0;
}


namespace Genode {

	struct Allocator_tracer : Allocator
	{
		size_t     _sum;
		Allocator &_wrapped;

		Allocator_tracer(Allocator &wrapped) : _sum(0), _wrapped(wrapped) { }

		size_t sum() const { return _sum; }

		bool alloc(size_t size, void **out_addr)
		{
			_sum += size;
			return _wrapped.alloc(size, out_addr);
		}

		void free(void *addr, size_t size)
		{
			_sum -= size;
			_wrapped.free(addr, size);
		}

		size_t overhead(size_t size) const override { return _wrapped.overhead(size); }
		bool    need_size_for_free() const override { return _wrapped.need_size_for_free(); }
	};
};


namespace Genode {

	/**
	 * Helper for the formatted output of a chunk state
	 */
	static inline void print(Output &out, File_system::Chunk_level_0 const &chunk)
	{
		using namespace File_system;
	
		static char read_buf[Chunk_level_0::SIZE];
	
		size_t used_size = chunk.used_size();
	
		struct File_size_out_of_bounds { };
		if (used_size > Chunk_level_0::SIZE)
			throw File_size_out_of_bounds();
	
		chunk.read(read_buf, used_size, 0);
	
		Genode::print(out, "content (size=", used_size, "): ");

		Genode::print(out, "\"");
		for (unsigned i = 0; i < used_size; i++) {
			char const c = read_buf[i];
			if (c)
				Genode::print(out, Genode::Char(c));
			else
				Genode::print(out, ".");
		}
		Genode::print(out, "\"");
	}
}


static void write(File_system::Chunk_level_0 &chunk,
                  char const *str, Genode::off_t seek_offset)
{
	chunk.write(str, Genode::strlen(str), seek_offset);
	Genode::log("write \"", str, "\" at offset ", seek_offset, " -> ", chunk);
}


static void truncate(File_system::Chunk_level_0 &chunk,
                     File_system::file_size_t size)
{
	chunk.truncate(size);
	Genode::log("trunc(", size, ") -> ", chunk);
}


int main(int, char **)
{
	using namespace File_system;
	using namespace Genode;

	log("--- ram_fs_chunk test ---");

	log("chunk sizes");
	log("  level 0: payload=", (int)Chunk_level_0::SIZE, " sizeof=", sizeof(Chunk_level_0));
	log("  level 1: payload=", (int)Chunk_level_1::SIZE, " sizeof=", sizeof(Chunk_level_1));
	log("  level 2: payload=", (int)Chunk_level_2::SIZE, " sizeof=", sizeof(Chunk_level_2));
	log("  level 3: payload=", (int)Chunk_level_3::SIZE, " sizeof=", sizeof(Chunk_level_3));

	static Allocator_tracer alloc(*env()->heap());

	{
		Chunk_level_0 chunk(alloc, 0);

		write(chunk, "five-o-one", 0);

		/* overwrite part of the file */
		write(chunk, "five", 7);

		/* write to position beyond current file length */
		write(chunk, "Nuance", 17);
		write(chunk, "YM-2149", 35);

		truncate(chunk, 30);

		for (unsigned i = 29; i > 0; i--)
			truncate(chunk, i);
	}

	log("allocator: sum=", alloc.sum());

	return 0;
}
