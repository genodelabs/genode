/*
 * \brief  Test component for the watch feature of the `lx_fs` server.
 * \author Pirmin Duss
 * \date   2020-06-17
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* libc includes */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* stdcxx includes */
#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>


void print_usage(const char *prg)
{
	std::cerr << "Usage: " << prg << " <--fwrite|--write> <input_file_name> <output_file_name>\n\n";
	std::cerr << "  --fwrite           use fopen/fwrite/fclose functions\n";
	std::cerr << "  --write            use open/write/close functions\n";
	std::cerr << "  input_file_name    name of the input file to write.\n";
	std::cerr << "  output_file_name   name of the file to write to.\n";
	std::cerr << "\n" << std::endl;

	exit(-1);
}


void use_fwrite(std::string const &data, char const *out_file_name)
{
	FILE  *out_fp   { fopen(out_file_name, "w") };
	if (out_fp == nullptr) {
		exit(-88);
	}

	auto   to_write { data.size() };
	size_t pos      { 0 };

	while (to_write > 0) {

		auto written { fwrite(data.c_str()+pos, sizeof(char), to_write, out_fp) };

		to_write -= written;
		pos      += written;
	}

	fclose(out_fp);
}


void use_write(std::string const &data, char const *out_file_name)
{
	auto fd { open(out_file_name, O_WRONLY) };
	if (fd < 0) {
		exit(-99);
	}

	auto   to_write { data.size() };
	size_t pos      { 0 };

	while (to_write > 0) {

		auto written { write(fd, data.c_str()+pos, to_write) };

		if (written < 0) {
			exit(-66);
		}

		to_write -= written;
		pos      += written;
	}

	close(fd);
}


std::string read_input(char const *file_name)
{
	std::ifstream in_stream { file_name };

	return std::string(std::istreambuf_iterator<char>(in_stream),
	                   std::istreambuf_iterator<char>());
}


int main(int argc, char *argv[])
{
	if (argc < 4) {
		print_usage(argv[0]);
	}

	std::string const data { read_input(argv[2]) };

	if (strcmp(argv[1], "--fwrite") == 0) {
		use_fwrite(data, argv[3]);
	} else if (strcmp(argv[1], "--write") == 0) {
		use_write(data, argv[3]);
	} else {
		print_usage(argv[0]);
	}

	return 0;
}
