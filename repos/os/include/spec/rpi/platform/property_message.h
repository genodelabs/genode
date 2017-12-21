/*
 * \brief  Marshalling of mbox messages for property channel
 * \author Norman Feske
 * \date   2013-09-15
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PLATFORM__PROPERTY_MESSAGE_H_
#define _PLATFORM__PROPERTY_MESSAGE_H_

/* Genode includes */
#include <util/misc_math.h>
#include <base/printf.h>

/* board-specific includes */
#include <drivers/defs/rpi.h>

namespace Platform {
	using namespace Genode;
	struct Property_message;
}


/**
 * Mailbox message buffer for the property channel
 *
 * This data structure is overlayed with memory shared with the VC. It
 * contains a header, followed by a sequence of so-called command tags, wrapped
 * up by a zero as an end marker.
 */
struct Platform::Property_message
{
	uint32_t buf_size = 0;

	enum Code { REQUEST          = 0,
	            RESPONSE_SUCCESS = 0x80000000 };

	Code code = REQUEST;

	/*
	 * Start of the buffer that contains a sequence of tags
	 */
	char buffer[0];

	/*
	 * There must be no member variables after this point
	 */

	/*
	 * Each tag consists of a header, a part with request arguments, and a
	 * part with responses.
	 */
	template <typename TAG>
	struct Tag
	{
		uint32_t const opcode;

		/**
		 * Size of tag buffer
		 */
		uint32_t const buf_size;

		/**
		 * Size of request part of the tag
		 *
		 * The value is never changed locally. Therefore, it is declared as
		 * const. However, it will be updated by the VC. So we declare it
		 * volatile, too.
		 */
		uint32_t volatile const len;

		char payload[0];

		/**
		 * Utility for returning a response size of a tag type
		 *
		 * Depending on the presence of a 'TAG::Response' type, we need to
		 * return the actual size of the response (if the type is present) or
		 * 0. Both overloads are called with a compliant parameter 0. But only
		 * if 'T::Response' exists, the first overload is selected.
		 *
		 * SFINAE to the rescue!
		 */
		template <typename T>
		static size_t response_size(typename T::Response *)
		{
			return sizeof(typename T::Response);
		}

		template <typename>
		static size_t response_size(...)
		{
			return 0;
		}

		template <typename T>
		static size_t request_size(typename T::Request *)
		{
			return sizeof(typename T::Request);
		}

		template <typename>
		static size_t request_size(...)
		{
			return 0;
		}

		template <typename T>
		struct Placeable : T
		{
			template <typename... ARGS>
			Placeable(ARGS... args) : T(args...) { }

			inline void *operator new (__SIZE_TYPE__, void *ptr) { return ptr; }
		};

		template <typename T, typename... ARGS>
		void construct_request(typename T::Request *, ARGS... args)
		{
			new ((typename T::Request *)payload)
				Placeable<typename T::Request>(args...);
		}

		template <typename>
		void construct_request(...) { }

		template <typename T>
		void construct_response(typename T::Response *)
		{
			new (payload) Placeable<typename T::Response>;
		}

		template <typename>
		void construct_response(...) { }

		static constexpr size_t payload_size()
		{
			return max(request_size<TAG>(0), response_size<TAG>(0));
		}

		template <typename... REQUEST_ARGS>
		Tag(REQUEST_ARGS... request_args)
		:
			opcode(TAG::opcode()),
			buf_size(payload_size()),
			len(request_size<TAG>(0))
		{
			/*
			 * The order is important. If we called 'construct_response' after
			 * 'construct_request', we would overwrite the request parameters
			 * with the default response.
			 */
			construct_response<TAG>(0);
			construct_request<TAG>(0, request_args...);
		}

		inline void *operator new (__SIZE_TYPE__, void *ptr) { return ptr; }
	};

	void reset()
	{
		buf_size = 0;
		code     = REQUEST;
	}

	/**
	 * \return reference to tag in the message buffer
	 */
	template <typename POLICY, typename... REQUEST_ARGS>
	typename POLICY::Response const &append(REQUEST_ARGS... request_args)
	{
		auto *tag = new (buffer + buf_size) Tag<POLICY>(request_args...);

		buf_size += sizeof(Tag<POLICY>) + Tag<POLICY>::payload_size();

		return *(typename POLICY::Response *)tag->payload;
	}

	template <typename POLICY, typename... REQUEST_ARGS>
	void append_no_response(REQUEST_ARGS... request_args)
	{
		new (buffer + buf_size) Tag<POLICY>(request_args...);

		buf_size += sizeof(Tag<POLICY>) + Tag<POLICY>::payload_size();
	}

	void finalize()
	{
		/* append end tag */
		*(uint32_t *)(buffer + buf_size) = 0;
		buf_size += sizeof(uint32_t);
	}

	static unsigned channel() { return 8; }

	static Rpi::Videocore_cache_policy cache_policy()
	{
		return Rpi::NON_COHERENT; /* for channel 8 only */
	}

	void dump(char const *label)
	{
		unsigned const *buf = (unsigned *)this;
		printf("%s message:\n", label);
		for (unsigned i = 0;; i++) {
			for (unsigned j = 0; j < 8; j++) {
				unsigned const msg_word_idx = i*8 + j;
				printf(" %08x", buf[msg_word_idx]);
				if (msg_word_idx*sizeof(unsigned) < buf_size)
					continue;
				printf("\n");
				return;
			}
		}
	}

	inline void *operator new (__SIZE_TYPE__, void *ptr) { return ptr; }
};

#endif /* _PLATFORM__PROPERTY_MESSAGE_H_ */
