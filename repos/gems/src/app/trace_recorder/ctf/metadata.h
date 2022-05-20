/*
 * \brief  Metadata file writer
 * \author Johannes Schlatow
 * \date   2021-12-02
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__METADATA_H_
#define _CTF__METADATA_H_

#include <os/vfs.h>
#include <base/attached_rom_dataspace.h>
#include <base/buffered_output.h>

namespace Ctf {
	using namespace Genode;

	class Metadata;
}

class Ctf::Metadata
{
	private:
		Attached_rom_dataspace    &_metadata_rom;
		uint64_t                   _timestamp_freq;

		template <typename PROLOGUE, typename EPILOGUE>
		bool _with_metadata_rom(PROLOGUE && prologue,
		                        EPILOGUE && epilogue) const
		{
			using Token = Genode::Token<Genode::Scanner_policy_identifier_with_underline>;
			char const * rom_start = _metadata_rom.local_addr<char>();
			size_t const rom_size  = _metadata_rom.size();

			Token token { rom_start, rom_size };

			for (; token; token = token.next()) {
				if (token.type() == Token::IDENT) {
					if (token.matches("freq") && token.len() == 4) {

						prologue(rom_start, token.next().start() - rom_start);

						token = token.next_after("\n");

						/* find null termination */
						char const * rom_end = token.start();
						for (; rom_end < rom_start+rom_size && rom_end && *rom_end; rom_end++);

						epilogue(token.start(), rom_end - token.start());

						return true;
					}
				}
			}

			error("Error parsing metadata ROM. Could not find 'freq' definition.");
			return false;
		}

	public:

		Metadata(Attached_rom_dataspace & metadata_rom, uint64_t freq)
		: _metadata_rom(metadata_rom),
		  _timestamp_freq(freq)
		{ }

		void write_file(Genode::New_file & dst) const
		{
			using namespace Genode;

			bool write_error = false;

			auto write = [&] (char const *str)
			{
				if (dst.append(str, strlen(str)) != New_file::Append_result::OK)
					write_error = true;
			};
			Buffered_output<32, decltype(write)> output(write);

			_with_metadata_rom(
				/* prologue, up to 'freq' */
				[&] (char const * start, size_t len) {
					if (dst.append(start, len) != New_file::Append_result::OK)
						write_error = true;

					print(output, " = ", _timestamp_freq, ";\n");
				},
				/* epilogue, everything after '\n' */
				[&] (char const * start, size_t len) {
					if (dst.append(start, len) != New_file::Append_result::OK)
						write_error = true;
				});

			if (write_error)
				error("Write to 'metadata' failed");
		}
};


#endif /* _CTF__METADATA_H_ */
