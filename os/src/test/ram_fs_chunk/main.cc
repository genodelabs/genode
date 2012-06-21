/*
 * \brief  Unit test for RAM fs chunk data structure
 * \author Norman Feske
 * \date   2012-04-19
 */

/* Genode includes */
#include <base/env.h>
#include <base/printf.h>

/* local 'ram_fs' include */
#include <chunk.h>

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

		size_t overhead(size_t size) { return 0; }
	};
};


static void dump(File_system::Chunk_level_0 &chunk)
{
	using namespace File_system;

	static char read_buf[Chunk_level_0::SIZE];

	size_t used_size = chunk.used_size();

	struct File_size_out_of_bounds { };
	if (used_size > Chunk_level_0::SIZE)
		throw File_size_out_of_bounds();

	chunk.read(read_buf, used_size, 0);

	printf("content (size=%zd): \"", used_size);
	for (unsigned i = 0; i < used_size; i++) {
		char c = read_buf[i];
		if (c)
			printf("%c", c);
		else
			printf(".");
	}
	printf("\"\n");
}


static void write(File_system::Chunk_level_0 &chunk,
                  char const *str, Genode::off_t seek_offset)
{
	using namespace Genode;
	printf("write \"%s\" at offset %ld -> ", str, seek_offset);
	chunk.write(str, strlen(str), seek_offset);
	dump(chunk);
}


static void truncate(File_system::Chunk_level_0 &chunk,
                     File_system::file_size_t size)
{
	using namespace Genode;
	printf("trunc(%zd) -> ", (size_t)size);
	chunk.truncate(size);
	dump(chunk);
}


int main(int, char **)
{
	using namespace File_system;
	using namespace Genode;

	printf("--- ram_fs_chunk test ---\n");

	PINF("chunk sizes");
	PINF("  level 0: payload=%d sizeof=%zd", Chunk_level_0::SIZE, sizeof(Chunk_level_0));
	PINF("  level 1: payload=%d sizeof=%zd", Chunk_level_1::SIZE, sizeof(Chunk_level_1));
	PINF("  level 2: payload=%d sizeof=%zd", Chunk_level_2::SIZE, sizeof(Chunk_level_2));
	PINF("  level 3: payload=%d sizeof=%zd", Chunk_level_3::SIZE, sizeof(Chunk_level_3));

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

	printf("allocator: sum=%zd\n", alloc.sum());

	return 0;
}
