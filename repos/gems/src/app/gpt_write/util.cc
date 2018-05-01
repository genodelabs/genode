/*
 * \brief  Random utility
 * \author Josef Soentgen
 * \date   2018-05-05
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/allocator.h>

/* local includes */
#include <util.h>

/* library includes */
#include <jitterentropy.h>


static struct rand_data *_ec_stir;


/**
 * Initialize random back end
 *
 * \param alloc  reference to allocator used internally
 *
 * \throw Init_random_failed
 */
void Util::init_random(Genode::Allocator &alloc)
{
	struct Init_random_failed : Genode::Exception { };

	/* initialize private allocator backend */
	jitterentropy_init(alloc);

	int err = jent_entropy_init();
	if (err) {
		Genode::error("jitterentropy library could not be initialized!");
		throw Init_random_failed();
	}

	/* use the default behaviour as specified in jitterentropy(3) */
	_ec_stir = jent_entropy_collector_alloc(0, 0);
	if (!_ec_stir) {
		Genode::error("jitterentropy could not allocate entropy collector!");
		throw Init_random_failed();
	}
}


/**
 * Fill buffer with requested number of random bytes
 *
 * \param dest  pointer to destination buffer
 * \param len   size of the destination buffer
 *
 * \throw Could_not_harvest_enough_randomness
 */
void Util::get_random(Genode::uint8_t *dest, Genode::size_t len)
{
	struct Could_not_harvest_enough_randomness : Genode::Exception { };

	if (jent_read_entropy(_ec_stir, (char*)dest, len) < 0) {
		throw Could_not_harvest_enough_randomness();
	}
}


/**
 * Convert size string
 *
 * \param size  reference to size string
 *
 * \return converted size in bytes
 */
Genode::uint64_t Util::convert(Util::Size_string const &size)
{
	if (!size.valid()) { return 0; }

	Genode::uint64_t length = 0;
	enum { MAX_SIZE = ~0ull, };
	if (size == "max") {
		length = MAX_SIZE;
	} else {
		Util::Number_of_bytes bytes;
		Util::ascii_to(size.string(), bytes);
		length = bytes;
	}

	return length;
}


/**
 * Align LBA at alignment boundary
 *
 * \param block_size  used to calculate number of LBAs
 * \param alignment   alignment in number of bytes
 * \param lba         start LBA
 *
 * \return returns aligned start LBA
 */
Block::sector_t Util::align_start(Genode::size_t block_size,
                                  Genode::size_t alignment,
                                  Block::sector_t lba)
{
	struct Invalid_alignment : Genode::Exception { };
	if (alignment < block_size || !block_size || !alignment) {
		throw Invalid_alignment();
	}

	Block::sector_t const blocks = alignment / block_size;
	return Genode::align_addr(lba, Genode::log2(blocks));
}


/**
 * Convert size in bytes to number of LBAs
 */
Block::sector_t Util::size_to_lba(Genode::size_t block_size, Genode::uint64_t size)
{
	/* XXX align/round-down etc. */
	return size / block_size;
}


/**
 * Simple bitwise CRC32 checking
 *
 * \param buf   pointer to buffer containing data
 * \param size  length of buffer in bytes
 *
 * \return CRC32 checksum of data
 */
Genode::uint32_t Util::crc32(void const * const buf, Genode::size_t size)
{
	Genode::uint8_t const *p = static_cast<Genode::uint8_t const*>(buf);
	Genode::uint32_t crc = ~0U;

	while (size--) {
		crc ^= *p++;
		for (Genode::uint32_t j = 0; j < 8; j++)
			crc = (-Genode::int32_t(crc & 1) & 0xedb88320) ^ (crc >> 1);
	}

	return crc ^ ~0U;
}


/**
 * Extract all valid ASCII characters from UTF-16LE buffer
 *
 * The function operates in a rather crude way and just tries to extract all
 * characters < 128, even non-printable ones.
 *
 * \param dest      pointer to destination buffer
 * \param dest_len  length of the destination buffer in bytes
 * \param src       pointer to source buffer
 * \param dest_len  length of the source buffer in 2 bytes
 *
 * \return length of resulting ASCII string
 */
Genode::size_t Util::extract_ascii(char *dest, size_t dest_len,
                                   uint16_t const *src, size_t src_len)
{
	char *p = dest;
	size_t j = 0;
	size_t i = 0;

	for (size_t u = 0; u < src_len && src[u] != 0; u++) {
		uint32_t utfchar = src[i++];

		if ((utfchar & 0xf800) == 0xd800) {
			unsigned int c = src[i];
			if ((utfchar & 0x400) != 0 || (c & 0xfc00) != 0xdc00) {
				utfchar = 0xfffd;
			} else {
				i++;
			}
		}

		p[j] = (utfchar < 0x80) ? utfchar : '.';
		/* leave space for NUL */
		if (++j == dest_len - 1) { break; }
	}

	p[j] = 0;
	return j;
}


/**
 * Convert printable ASCII characters to UTF-16LE
 *
 * The function operates in a rather crude way and will
 * truncate the input string if it does not fit.
 *
 * \param dest      pointer to destination buffer
 * \param dest_len  length of the destination buffer in 16bit words
 * \param src       pointer to source buffer
 * \param dest_len  length of the source buffer in 8bit words
 *
 * \return length of resulting UTF-16 string
 */
Genode::size_t Util::convert_ascii(uint16_t *dest, size_t dest_len,
                                   uint8_t const *src, size_t src_len)
{
	Genode::memset(dest, 0, dest_len * sizeof(uint16_t));

	if (src_len / sizeof(uint16_t) > dest_len) {
		Genode::warning("input too long, will be truncated");
		src_len = dest_len;
	}

	size_t i = 0;
	for (; i < src_len; i++) {
		uint16_t const utfchar = src[i];
		dest[i] = utfchar;
	}

	return i;
}
