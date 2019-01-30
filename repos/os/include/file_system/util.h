/*
 * \brief  Utilities
 * \author Norman Feske
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _FILE_SYSTEM__UTIL_H_
#define _FILE_SYSTEM__UTIL_H_

#include <file_system_session/file_system_session.h>
#include <os/path.h>

namespace File_system {

	/**
	 * Return true if character 'c' occurs in null-terminated string 'str'
	*/
	inline bool string_contains(char const *str, char c)
	{
		for (; *str; str++)
			if (*str == c)
				return true;
		return false;
	}

	/**
	 * Return base-name portion of null-terminated path string
	 */
	static inline char const *basename(char const *path)
	{
		char const *start = path;

		for (; *path; path++)
			if (*path == '/')
				start = path + 1;

		return start;
	}


	/**
	 * Return true if specified path contains at least path delimiters
	 */
	static inline bool contains_path_delimiter(char const *path)
	{
		for (; *path; path++)
			if (*path == '/')
				return true;

		return false;
	}


	/**
	 * Return true if 'str' is a valid node name
	 */
	static inline bool valid_name(char const *str)
	{
		if (string_contains(str, '/')) return false;

		/* must have at least one character */
		if (str[0] == 0) return false;

		return true;
	}


	/**
	 * Open a directory, ensuring all parent directories exists.
	 */
	static inline Dir_handle ensure_dir(Session &fs, char const *path)
	{
		try {
			return fs.dir(path, false);
		} catch (Lookup_failed) {
			try {
				return fs.dir(path, true);
			} catch (Lookup_failed) {
				Genode::Path<MAX_PATH_LEN> target(path);
				target.strip_last_element();
				fs.close(ensure_dir(fs, target.base()));
			}
		}
		return fs.dir(path, true);
	}


	/**
	 * Collect pending packet acknowledgements, freeing the space occupied
	 * by the packet in the bulk buffer
	 *
	 * This function should be called prior enqueing new packets into the
	 * packet stream to free up space in the bulk buffer.
	 */
	static void collect_acknowledgements(Session::Tx::Source &source)
	{
		while (source.ack_avail())
			source.release_packet(source.get_acked_packet());
	}


	/**
	 * Read file content
	 */
	static inline size_t read(Session &fs, Node_handle const &node_handle,
	                          void *dst, size_t count, seek_off_t seek_offset = 0)
	{
		bool success = true;
		Session::Tx::Source &source = *fs.tx();

		size_t const max_packet_size = source.bulk_buffer_size() / 2;

		size_t remaining_count = count;

		while (remaining_count && success) {

			collect_acknowledgements(source);

			size_t const curr_packet_size =
				Genode::min(remaining_count, max_packet_size);

			Packet_descriptor
				packet(source.alloc_packet(curr_packet_size),
				       node_handle,
				       File_system::Packet_descriptor::READ,
				       curr_packet_size,
				       seek_offset);

			/* pass packet to server side */
			source.submit_packet(packet);

			packet = source.get_acked_packet();
			success = packet.succeeded();

			size_t const read_num_bytes =
				Genode::min(packet.length(), curr_packet_size);

			/* copy-out payload into destination buffer */
			Genode::memcpy(dst, source.packet_content(packet), read_num_bytes);

			source.release_packet(packet);

			/* prepare next iteration */
			seek_offset += read_num_bytes;
			dst = (void *)((Genode::addr_t)dst + read_num_bytes);
			remaining_count -= read_num_bytes;

			/*
			 * If we received less bytes than requested, we reached the end
			 * of the file.
			 */
			if (read_num_bytes < curr_packet_size)
				break;
		}

		return count - remaining_count;
	}


	/**
	 * Write file content
	 */
	static inline size_t write(Session &fs, Node_handle const &node_handle,
	                          void const *src, size_t count, seek_off_t seek_offset = 0)
	{
		bool success = true;
		Session::Tx::Source &source = *fs.tx();

		size_t const max_packet_size = source.bulk_buffer_size() / 2;

		size_t remaining_count = count;

		while (remaining_count && success) {

			collect_acknowledgements(source);

			size_t const curr_packet_size =
				Genode::min(remaining_count, max_packet_size);

			Packet_descriptor
				packet(source.alloc_packet(curr_packet_size),
				       node_handle,
				       File_system::Packet_descriptor::WRITE,
				       curr_packet_size,
				       seek_offset);

			/* copy-out source buffer into payload */
			Genode::memcpy(source.packet_content(packet), src, curr_packet_size);

			/* pass packet to server side */
			source.submit_packet(packet);

			packet = source.get_acked_packet();;
			success = packet.succeeded();
			source.release_packet(packet);

			/* prepare next iteration */
			seek_offset += curr_packet_size;
			src = (void *)((Genode::addr_t)src + curr_packet_size);
			remaining_count -= curr_packet_size;
		}

		return count - remaining_count;
	}


	class Handle_guard
	{
		private:

			Session     &_session;
			Node_handle  _handle;

		public:

			Handle_guard(Session &session, Node_handle handle)
			: _session(session), _handle(handle) { }

			~Handle_guard() { _session.close(_handle); }
	};

	class Packet_guard
	{
		private:

			Session::Tx::Source           &_source;
			File_system::Packet_descriptor _packet;

		public:

			Packet_guard(Session::Tx::Source           &source,
			             File_system::Packet_descriptor packet)
			: _source(source), _packet(packet) { }

			~Packet_guard()
			{
				_source.release_packet(_packet);
			}
	};

}

#endif /* _FILE_SYSTEM__UTIL_H_ */
