/*
 * \brief  Types used by trace policy for pcapng events
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_RECORDER_POLICY__PCAPNG_H_
#define _TRACE_RECORDER_POLICY__PCAPNG_H_

#include <util/construct_at.h>
#include <util/string.h>
#include <trace/timestamp.h>
#include <trace_recorder_policy/event.h>

namespace Pcapng {
	using namespace Genode;

	/* Link type as defined in Interface Description Block */
	enum Link_type { ETHERNET = 1 };

	struct Interface_name;
	struct Traced_packet;
}

namespace Trace_recorder {
	using namespace Genode;

	class Pcapng_event;
};


struct Pcapng::Interface_name
{
	enum : uint8_t { MAX_NAME_LEN = 40 };

	uint16_t const _link_type;
	uint8_t        _name_len;
	char           _name[0] { };

	Interface_name(Link_type type, bool out, char const *cstr)
	: _link_type(type),
	  _name_len((uint8_t)(Genode::Cstring(cstr, MAX_NAME_LEN-5).length() + 1))
	{
		copy_cstring(_name, cstr, _name_len);
		if (out) {
			copy_cstring(&_name[_name_len-1], "_out", 5);
			_name_len += 4;
		} else {
			copy_cstring(&_name[_name_len-1], "_in", 4);
			_name_len += 3;
		}
	}
	
	/* length including null-termination */
	uint8_t data_length() const { return _name_len; }

	char const *string()  const { return _name; }

} __attribute__((packed));


/**
 * Struct capturing a traced packet. Intended to be easily convertible into an Enhanced_packet_block.
 */
struct Pcapng::Traced_packet
{
	uint32_t _captured_length;
	uint32_t _original_length;
	uint32_t _packet_data[0] { };

	Traced_packet(uint32_t packet_size, void *packet_ptr, uint32_t max_captured_length)
	: _captured_length(min(max_captured_length, packet_size)),
	  _original_length(packet_size)
	{ memcpy(_packet_data, packet_ptr, _captured_length); }

	/* copy constructor */
	Traced_packet(Traced_packet const &packet)
	: _captured_length(packet._captured_length),
	  _original_length(packet._original_length)
	{ memcpy(_packet_data, packet._packet_data, _captured_length); }

	uint32_t data_length() const { return _captured_length; }

	size_t  total_length() const { return sizeof(Traced_packet) + _captured_length; }

} __attribute__((packed));


/**
 * Struct used by trace policy, bundles Timestamp, Interface_name and Traced_packet.
 */
class Trace_recorder::Pcapng_event : public Trace_event<Event_type::PCAPNG>
{
	private:
		using Interface_name = Pcapng::Interface_name;
		using Traced_packet  = Pcapng::Traced_packet;

		Trace::Timestamp const _timestamp { Trace::timestamp() };
		Interface_name   const _interface;

		/* _interface must be the last member since it has variable size */

		void       *_data()       { return (void*)((addr_t)this + sizeof(Pcapng_event) + _interface.data_length()); }
		void const *_data() const { return (void*)((addr_t)this + sizeof(Pcapng_event) + _interface.data_length()); }

	public:

		static Event_type Type() { return Event_type::PCAPNG; }

		static size_t max_size(size_t max_capture_len) {
			return sizeof(Pcapng_event) + Interface_name::MAX_NAME_LEN + sizeof(Traced_packet) + max_capture_len; }

		Pcapng_event(Pcapng::Link_type type, char const *cstr, bool out, uint32_t packet_sz, void *packet_ptr, uint32_t max_captured_len)
		: _interface(type, out, cstr)
		{
			/* construct Traced_packet after Interface_name */
			construct_at<Traced_packet>(_data(), packet_sz, packet_ptr, max_captured_len);
		}

		Traced_packet    const &packet()       const { return *reinterpret_cast<Traced_packet const*>(_data()); }
		Interface_name   const &interface()    const { return _interface; }
		Trace::Timestamp        timestamp()    const { return _timestamp; }
		size_t                  total_length() const {
			return sizeof(Pcapng_event) + _interface.data_length() + packet().total_length(); }

} __attribute__((packed));

#endif /* _TRACE_RECORDER_POLICY__PCAPNG_H_ */
