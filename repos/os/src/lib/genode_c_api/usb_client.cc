/*
 * \brief  Genode USB client provider C-API
 * \author Sebastian Sumpf
 * \date   2023-06-29
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/id_space.h>
#include <usb_session/connection.h>
#include <base/heap.h>
#include <genode_c_api/usb_client.h>
#include <util/bit_allocator.h>

using namespace Genode;

struct Usb_client;
struct Usb_completion;

using Usb_id_space = Id_space<Usb_client>;


struct Usb_completion : Usb::Completion
{
	Usb::Packet_descriptor packet { };

	genode_usb_client_request_packet *request = nullptr;

	void complete(Usb::Packet_descriptor &p) override
	{
		packet = p;
		request->actual_length = (packet.type == Usb::Packet_descriptor::CTRL) ?
		                         packet.control.actual_size : packet.transfer.actual_size;

		request->error = NO_ERROR;

		if (!packet.succeded) {
			switch (packet.error) {
			case Usb::Packet_descriptor::NO_ERROR:
				request->error = NO_ERROR; break;
			case Usb::Packet_descriptor::INTERFACE_OR_ENDPOINT_ERROR:
				request->error = INTERFACE_OR_ENDPOINT_ERROR; break;
			case Usb::Packet_descriptor::MEMORY_ERROR:
				request->error = MEMORY_ERROR; break;
			case Usb::Packet_descriptor::NO_DEVICE_ERROR:
				request->error = NO_DEVICE_ERROR; break;
			case Usb::Packet_descriptor::PACKET_INVALID_ERROR:
				request->error = PACKET_INVALID_ERROR; break;
			case Usb::Packet_descriptor::PROTOCOL_ERROR:
				request->error = PROTOCOL_ERROR; break;
			case Usb::Packet_descriptor::STALL_ERROR:
				request->error = STALL_ERROR; break;
			case Usb::Packet_descriptor::TIMEOUT_ERROR:
				request->error = TIMEOUT_ERROR; break;
			case Usb::Packet_descriptor::UNKNOWN_ERROR:
				request->error = UNKNOWN_ERROR; break;
			}
		}

		if (request->complete_callback)
			request->complete_callback(request);
	}

	void free()
	{
		request->error = NO_DEVICE_ERROR;
		if (request->free_callback) request->free_callback(request);
	}
};


struct Usb_client : Usb::Connection
{
	enum { PACKET_SLOTS = Usb::Session::TX_QUEUE_SIZE };

	Usb_completion              completions[PACKET_SLOTS];
	Bit_allocator<PACKET_SLOTS> slots { };
	Usb_id_space::Element const elem;

	Usb_client(Env &env, Usb_id_space &space, char const *label,
	           Range_allocator *alloc,
	           Signal_context_capability state_changed)
	: Usb::Connection(env, alloc, label, 512*1024, state_changed),
	  elem(*this, space)
	{ }

	genode_usb_client_handle_t handle() const { return elem.id().value; }

	void free_completion(Usb_completion *completion)
	{
		unsigned long slot_idx = (unsigned long)(completion - completions);
		slots.free(slot_idx);
	}

	Usb_completion *alloc(size_t size)
	{
		/*
		 * We don't need to check for 'ready_to_submit' because we have as many
		 * compltions as packet slots
		 */
		Usb_completion *completion = nullptr;
		try {
			completion = &completions[slots.alloc()];
			Usb::Packet_descriptor packet = Usb::Session_client::alloc_packet(size);
			packet.completion  = completion;
			completion->packet = packet;
		} catch (Tx::Source::Packet_alloc_failed) {
			free_completion(completion);
			return nullptr;
		} catch (Bit_allocator<PACKET_SLOTS>::Out_of_indices) {
			return nullptr;
		}

		return completion;
	}

	void release(Usb_completion *completion)
	{
		source()->release_packet(completion->packet);
		free_completion(completion);
	}
};


static Usb_id_space _usb_space { };

template <typename FUNC>
int usb_client_apply(genode_usb_client_handle_t handle, FUNC const &fn)
{
	Usb_id_space::Id id { .value = handle };
	try {
		_usb_space.apply<Usb_client>(id, fn);
	} catch (Usb_id_space::Id_space::Unknown_id) {
		error("Invalid handle: ", handle);
		return -1;
	}
	return 0;
}


genode_usb_client_handle_t
genode_usb_client_create(struct genode_env             *env,
                         struct genode_allocator       *md_alloc,
                         struct genode_range_allocator *alloc,
                         char const                    *label,
                         struct genode_signal_handler  *handler)
{
	Env             &_env      = *static_cast<Env *>(env);
	Range_allocator *_alloc    = static_cast<Range_allocator*>(alloc);
	Allocator       *_md_alloc = static_cast<Allocator *>(md_alloc);

	Usb_client *client = new (_md_alloc) Usb_client(_env, _usb_space, label,
	                                                _alloc, cap(handler));
	return client->handle();
}


void genode_usb_client_destroy(genode_usb_client_handle_t handle,
                               struct genode_allocator *md_alloc)
{
	usb_client_apply(handle, [&] (Usb_client &usb) {
		while(usb.source()->ack_avail()) {
			Usb::Packet_descriptor p = usb.source()->get_acked_packet();
			if (p.completion) static_cast<Usb_completion *>(p.completion)->free();
			usb.source()->release_packet(p);
		}
		destroy(md_alloc, &usb);
	});
};


void genode_usb_client_sigh_ack_avail(genode_usb_client_handle_t handle,
                                      struct genode_signal_handler *handler)
{
	usb_client_apply(handle, [&] (Usb_client &usb) {
		usb.tx_channel()->sigh_ack_avail(cap(handler)); });
}


int genode_usb_client_config_descriptor(genode_usb_client_handle_t handle,
                                        genode_usb_device_descriptor *device_descr,
                                        genode_usb_config_descriptor *config_descr)
{
	auto lambda = [&] (Usb_client &usb) {
		Usb::Device_descriptor dev_descr;
		Usb::Config_descriptor conf_descr;
		usb.config_descriptor(&dev_descr, &conf_descr);

		memcpy(device_descr, &dev_descr,    sizeof(*device_descr));
		memcpy(config_descr, &config_descr, sizeof(*config_descr));
	};

	return usb_client_apply(handle, lambda);
}


bool genode_usb_client_plugged(genode_usb_client_handle_t handle)
{
	bool plugged = false;
	usb_client_apply(handle, [&] (Usb_client &usb) {
		plugged = usb.plugged(); });
	
	return plugged;
}

void genode_usb_client_claim_interface(genode_usb_client_handle_t handle,
                                       unsigned interface_num)
{
	usb_client_apply(handle, [&] (Usb_client &usb) {
		usb.claim_interface(interface_num);
	});
}


void genode_usb_client_release_interface(genode_usb_client_handle_t handle,
                                         unsigned interface_num)
{
	usb_client_apply(handle, [&] (Usb_client &usb) {
		usb.release_interface(interface_num);
	});
}


bool genode_usb_client_request(genode_usb_client_handle_t        handle,
                               genode_usb_client_request_packet *request)
{
	Usb_completion *completion = nullptr;

	usb_client_apply(handle, [&] (Usb_client &usb) {
		completion = usb.alloc(request->buffer.size);

		if (completion)
			request->buffer.addr = usb.source()->packet_content(completion->packet);
	});

	if (!completion) return false;

	completion->request  = request;
	request->completion  = completion;

	/* setup packet */
	genode_request_packet_t *req    = &request->request;
	Usb::Packet_descriptor  *packet = &completion->packet;

	switch(req->type) {
	case IRQ:
		{
			packet->type = Usb::Packet_descriptor::IRQ;

			genode_usb_request_transfer *transfer = (genode_usb_request_transfer *)req->req;
			packet->transfer.polling_interval = transfer->polling_interval;
			packet->transfer.ep               = transfer->ep;
			break;
		}
	case CTRL:
		{
			packet->type = Usb::Packet_descriptor::CTRL;

			genode_usb_request_control *ctrl = (genode_usb_request_control *)req->req;
			packet->control.request      = ctrl->request;
			packet->control.request_type = ctrl->request_type;
			packet->control.value        = ctrl->value;
			packet->control.index        = ctrl->index;
			packet->control.timeout      = ctrl->timeout;
			break;
		}
	case BULK:
		{
			packet->type = Usb::Packet_descriptor::BULK;

			genode_usb_request_transfer *transfer = (genode_usb_request_transfer *)req->req;
			packet->transfer.ep = transfer->ep;
			break;
		}
	case ALT_SETTING:
		{
			genode_usb_altsetting *alt_setting = (genode_usb_altsetting*)req->req;

			packet->type = Usb::Packet_descriptor::ALT_SETTING;

			packet->interface.number      = alt_setting->interface_number;
			packet->interface.alt_setting = alt_setting->alt_setting;
			break;
		}

	case CONFIG:
		{
			genode_usb_config *config = (genode_usb_config *)req->req;

			packet->type   = Usb::Packet_descriptor::CONFIG;
			packet->number = config->value;
			break;
		}

	default:
		error("unknown USB client requested");
	};

	return true;
}


void genode_usb_client_request_submit(genode_usb_client_handle_t        handle,
                                      genode_usb_client_request_packet *request)
{
	Usb_completion *completion = static_cast<Usb_completion *>(request->completion);
	usb_client_apply(handle, [&] (Usb_client &usb) {
		usb.source()->submit_packet(completion->packet); });
}


void genode_usb_client_request_finish(genode_usb_client_handle_t               handle,
                                      struct genode_usb_client_request_packet *request)
{
	Usb_completion *completion = static_cast<Usb_completion *>(request->completion);
	usb_client_apply(handle, [&] (Usb_client &usb) {
		usb.release(completion); });
}


void genode_usb_client_execute_completions(genode_usb_client_handle_t handle)
{
	usb_client_apply(handle, [&] (Usb_client &usb) {
		while(usb.source()->ack_avail()) {
			Usb::Packet_descriptor p = usb.source()->get_acked_packet();
			if (p.completion) p.completion->complete(p);
		}
	});
}
