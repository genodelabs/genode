/*
 * \brief  PCAPNG backend
 * \author Johannes Schlatow
 * \date   2022-05-13
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <pcapng/backend.h>
#include <pcapng/enhanced_packet_block.h>
#include <pcapng/interface_description_block.h>
#include <pcapng/section_header_block.h>

using namespace Pcapng;
using Append_error  = Buffer::Append_error;
using Append_result = Buffer::Append_result;
using Pcapng_event  = Trace_recorder::Pcapng_event;


void Writer::start_iteration(Directory             &root,
                             Directory::Path const &path,
                             ::Subject_info  const &)
{
	/* write to '${path}.pcapng */
	Path<Directory::MAX_PATH_LEN> pcap_file { path };
	pcap_file.append(".pcapng");

	_file_path = Directory::Path(pcap_file.string());

	/* append to file */
	try {
		_dst_file.construct(root, _file_path, true);

		_interface_registry.clear();
		_buffer.clear();
		_buffer.append<Section_header_block>();
		_empty_section = true;
	}
	catch (New_file::Create_failed)  {
		error("Could not create file."); }
}


void Writer::process_event(Trace_recorder::Trace_event_base const &trace_event, size_t length)
{
	if (!_dst_file.constructed()) return;

	if (trace_event.type() != Trace_recorder::Event_type::PCAPNG) return;

	/* event is of type Pcapng::Trace_event */
	Pcapng_event const &event = trace_event.event<Pcapng_event>();

	/* map interface name to id of interface description block (IDB) */
	unsigned id          = 0;
	bool     buffer_full = false;
	_interface_registry.from_name(event.interface(),
		[&] (Interface const &iface) {
			/* IDB alread exists */
			id = iface.id();
		},
		[&] (Interface_name const &if_name, unsigned if_id) { /* IDB must be created */
			id = if_id;
			Append_result result =
				_buffer.append<Interface_description_block>(if_name,
				                                            Enhanced_packet_block::MAX_CAPTURE_LENGTH);

			result.with_error([&] (Append_error err) {
				switch (err)
				{
					case Append_error::OUT_OF_MEM:
						/* non-error, write to file and retry */
						buffer_full = true;
						break;
					case Append_error::OVERFLOW:
						error("Interface_description_block exceeds its MAX_SIZE");
						break;
				}
			});
			return !buffer_full;
		}
	);

	/* add enhanced packet block to buffer */
	if (!buffer_full) {
		uint64_t us_since_epoch = _ts_calibrator.epoch_from_timestamp_in_us(event.timestamp());
		Append_result result = _buffer.append<Enhanced_packet_block>(id, event.packet(), us_since_epoch);

		result.with_error([&] (Append_error err) {
			switch (err)
			{
				case Append_error::OUT_OF_MEM:
					/* non-error, write to file and retry */
					buffer_full = true;
					break;
				case Append_error::OVERFLOW:
					error("Enhanced_packet_block exceeds its MAX_SIZE");
					break;
			}
		});
	}

	/* write to file if buffer is full and process current event again */
	if (buffer_full) {
		_buffer.write_to_file(*_dst_file, _file_path);
		process_event(event, length);
	}
	else {
		_empty_section = false;
	}
}


void Writer::end_iteration()
{
	/* write buffer to file */
	if (!_empty_section)
		_buffer.write_to_file(*_dst_file, _file_path);

	_buffer.clear();
	_dst_file.destruct();
}


Trace_recorder::Writer_base &Backend::create_writer(Genode::Allocator             &alloc,
                                                    Genode::Registry<Writer_base> &registry,
                                                    Directory                     &,
                                                    Directory::Path      const    &)
{
	return *new (alloc) Writer(registry, _interface_registry, _buffer, _ts_calibrator);
}
