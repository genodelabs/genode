/*
 * \brief  Genode Audio out/in session + C-API implementation
 * \author Sebastian Sumpf
 * \date   2022-05-31
 */

/*
 * Copyright (C) 2022-2024 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

#include <audio_in_session/rpc_object.h>
#include <audio_out_session/rpc_object.h>
#include <record_session/connection.h>
#include <play_session/connection.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_label.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/log.h>
#include <os/reporter.h>
#include <root/component.h>

#include "audio.h"

using namespace Genode;

namespace Audio_out {
	class  Session_component;
	class  Out;
	class  Root;
	struct Root_policy;
	enum   Channel_number { LEFT, RIGHT, CHANNELS, INVALID = CHANNELS };
	static Session_component *channel_acquired[CHANNELS];
}


/**************
 ** Playback **
 **************/

class Audio_out::Session_component : public Audio_out::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Genode::Env &env, Channel_number channel, Signal_context_capability cap)
		:
			Session_rpc_object(env, cap), _channel(channel)
		{
			Audio_out::channel_acquired[_channel] = this;
		}

		~Session_component()
		{
			Audio_out::channel_acquired[_channel] = 0;
		}
};


class Audio_out::Out
{
	private:

		Genode::Env                            &_env;
		Genode::Signal_handler<Audio_out::Out>  _data_avail_dispatcher;

		Stream *left()  { return channel_acquired[LEFT]->stream(); }
		Stream *right() { return channel_acquired[RIGHT]->stream(); }

		void _advance_position(Packet *l, Packet *r)
		{
			bool full_left = left()->full();
			bool full_right = right()->full();

			left()->pos(left()->packet_position(l));
			right()->pos(right()->packet_position(r));

			left()->increment_position();
			right()->increment_position();

			Session_component *channel_left  = channel_acquired[LEFT];
			Session_component *channel_right = channel_acquired[RIGHT];

			if (full_left)
				channel_left->alloc_submit();

			if (full_right)
				channel_right->alloc_submit();
		}

		void _handle_data_avail() { }

		int16_t _data[4096];

	public:

		Out(Genode::Env &env)
		:
			_env(env),
			_data_avail_dispatcher(env.ep(), *this, &Audio_out::Out::_handle_data_avail)
		{ }

		static bool channel_number(const char     *name,
		                           Channel_number *out_number)
		{
			static struct Names {
				const char     *name;
				Channel_number  number;
			} names[] = {
				{ "left", LEFT }, { "front left", LEFT },
				{ "right", RIGHT }, { "front right", RIGHT },
				{ 0, INVALID }
			};

			for (Names *n = names; n->name; ++n)
				if (!Genode::strcmp(name, n->name)) {
					*out_number = n->number;
					return true;
				}

			return false;
		}

		Signal_context_capability data_avail() { return _data_avail_dispatcher; }

		genode_audio_packet record_packet()
		{
			genode_audio_packet packet = { nullptr, 0 };

			unsigned lpos = left()->pos();
			unsigned rpos = right()->pos();

			Packet *p_left  = left()->get(lpos);
			Packet *p_right = right()->get(rpos);

			packet.samples = genode_audio_samples_per_period();

			if (p_left->valid() && p_right->valid()) {
				/* convert float to S16LE interleaved */
				for (unsigned i = 0; i < packet.samples; i++) {
					_data[i * CHANNELS]     = int16_t(p_left->content()[i] * 32767);
					_data[i * CHANNELS + 1] = int16_t(p_right->content()[i] * 32767);
				}

				packet.data    = _data;

				p_left->invalidate();
				p_right->invalidate();

				p_left->mark_as_played();
				p_right->mark_as_played();
			} else {
				return packet;
			}

			_advance_position(p_left, p_right);

			/* always report when a period has passed */
			Session_component *channel_left  = channel_acquired[LEFT];
			Session_component *channel_right = channel_acquired[RIGHT];
			channel_left->progress_submit();
			channel_right->progress_submit();

			return packet;
		}
};


/**
 * Session creation policy for our service
 */
struct Audio_out::Root_policy
{
	using Result = Attempt<Ok, Session_error>;

	Result aquire(const char *args)
	{
		size_t ram_quota =
			Arg_string::find_arg(args, "ram_quota"  ).ulong_value(0);
		size_t session_size =
			align_addr(sizeof(Audio_out::Session_component), Align { .log2 = 12 });

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > ram_quota - session_size)) {
			Genode::error("insufficient 'ram_quota', got ", ram_quota,
			              " need ", sizeof(Stream) + session_size);
			return Session_error::INSUFFICIENT_RAM;
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!Out::channel_number(channel_name, &channel_number)) {
			Genode::error("invalid output channel '",(char const *)channel_name,"' requested, "
			              "denying '",Genode::label_from_args(args),"'");
			return Session_error::DENIED;
		}
		if (Audio_out::channel_acquired[channel_number]) {
			Genode::error("output channel '",(char const *)channel_name,"' is unavailable, "
			              "denying '",Genode::label_from_args(args),"'");
			return Session_error::DENIED;
		}

		return Ok();
	}

	void release() { }
};


namespace Audio_out {
	typedef Root_component<Session_component, Root_policy> Root_component;
}


/**
 * Root component, handling new session requests.
 */
class Audio_out::Root : public Audio_out::Root_component
{
	private:

		Genode::Env &_env;

		Signal_context_capability _cap;

	protected:

		Create_result _create_session(const char *args) override
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			Out::channel_number(channel_name, &channel_number);

			return *new (md_alloc())
				Session_component(_env, channel_number, _cap);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc,
		     Signal_context_capability cap)
		:
			Root_component(env.ep(), md_alloc),
			_env(env), _cap(cap)
		{ }
};


static bool _audio_out_active()
{
	using namespace Audio_out;
	return  channel_acquired[LEFT] && channel_acquired[RIGHT] &&
	        channel_acquired[LEFT]->active() && channel_acquired[RIGHT]->active();
}


/***************
 ** Recording **
 ***************/

namespace Audio_in {

	class Session_component;
	class In;
	class Root;
	struct Root_policy;
	static Session_component *channel_acquired;
	enum Channel_number { LEFT, CHANNELS, INVALID = CHANNELS };

}


class Audio_in::Session_component : public Audio_in::Session_rpc_object
{
	private:

		Channel_number _channel;

	public:

		Session_component(Genode::Env &env, Channel_number channel)
		: Session_rpc_object(env, Signal_context_capability()),
		  _channel(channel)
		{ channel_acquired = this; }

		~Session_component() { channel_acquired = nullptr; }
};


class Audio_in::In
{
	private:

		bool _active() { return channel_acquired && channel_acquired->active(); }

		Stream *stream() { return channel_acquired->stream(); }

	public:

		static bool channel_number(const char     *name,
		                           Channel_number *out_number)
		{
			static struct Names {
				const char     *name;
				Channel_number  number;
			} names[] = {
				{ "left", LEFT },
				{ 0, INVALID }
			};

			for (Names *n = names; n->name; ++n)
				if (!Genode::strcmp(name, n->name)) {
					*out_number = n->number;
					return true;
				}

			return false;
		}

		void play_packet(genode_audio_packet &packet)
		{
			if (!_active()) return;
			/*
			 * Check for an overrun first and notify the client later.
			 */
			bool overrun = stream()->overrun();

			Packet *p = stream()->alloc();

			float const scale = 32768.0f * 2;

			float * const content = p->content();
			short * const data    = packet.data;
			bzero(content, p->size());

			/* convert from 2 channels s16le interleaved to one channel float */
			for (unsigned long i = 0; i < packet.samples; i++) {
				float sample = data[i * 2] + data[i * 2 + 1];
				content[i]   = sample / scale;
			}

			stream()->submit(p);

			channel_acquired->progress_submit();

			if (overrun) channel_acquired->overrun_submit();
		}
};


struct Audio_in::Root_policy
{
	using Result = Attempt<Ok, Session_error>;

	Result aquire(char const *args)
	{
		size_t ram_quota = Arg_string::find_arg(args, "ram_quota").ulong_value(0);
		size_t session_size = align_addr(sizeof(Audio_in::Session_component), Align { .log2 = 12 });

		if ((ram_quota < session_size) ||
		    (sizeof(Stream) > (ram_quota - session_size))) {
			Genode::error("insufficient 'ram_quota', got ", ram_quota,
			              " need ", sizeof(Stream) + session_size,
			              ", denying '",Genode::label_from_args(args),"'");
			return Session_error::INSUFFICIENT_RAM;
		}

		char channel_name[16];
		Channel_number channel_number;
		Arg_string::find_arg(args, "channel").string(channel_name,
		                                             sizeof(channel_name),
		                                             "left");
		if (!In::channel_number(channel_name, &channel_number)) {
			Genode::error("invalid input channel '",(char const *)channel_name,"' requested, "
			              "denying '",Genode::label_from_args(args),"'");
			return Session_error::DENIED;
		}
		if (Audio_in::channel_acquired) {
			Genode::error("input channel '",(char const *)channel_name,"' is unavailable, "
			              "denying '",Genode::label_from_args(args),"'");
			return Session_error::DENIED;
		}

		return Ok();
	}

	void release() { }
};


namespace Audio_in {
	typedef Root_component<Session_component, Root_policy> Root_component;
}


/**
 * Root component, handling new session requests.
 */
class Audio_in::Root : public Audio_in::Root_component
{
	private:

		Genode::Env               &_env;

	protected:

		Create_result _create_session(char const *args) override
		{
			char channel_name[16];
			Channel_number channel_number = INVALID;
			Arg_string::find_arg(args, "channel").string(channel_name,
			                                             sizeof(channel_name),
			                                             "left");
			In::channel_number(channel_name, &channel_number);
			return *new (md_alloc()) Session_component(_env, channel_number);
		}

	public:

		Root(Genode::Env &env, Allocator &md_alloc)
		: Root_component(env.ep(), md_alloc), _env(env) { }
};


/*************************
 ** Record/Play session **
 *************************/

struct Stereo_output : Noncopyable
{
	static constexpr unsigned SAMPLES_PER_PERIOD = Audio_in::PERIOD;
	static constexpr unsigned CHANNELS = 2;

	Env &_env;

	Record::Connection _left  { _env, "left"  };
	Record::Connection _right { _env, "right" };

	struct Recording : private Noncopyable
	{
		bool depleted = false;

		/* 16 bit per sample, interleaved left and right */
		int16_t data[SAMPLES_PER_PERIOD*CHANNELS] { };

		void clear() { for (auto &e : data) e = 0; }

		void from_record_sessions(Record::Connection &left, Record::Connection &right)
		{
			using Samples_ptr = Record::Connection::Samples_ptr;

			Record::Num_samples const num_samples { SAMPLES_PER_PERIOD };

			auto clamped = [&] (float v)
			{
				return (v >  1.0) ?  1.0
				     : (v < -1.0) ? -1.0
				     :  v;
			};

			auto float_to_s16 = [&] (float v) { return int16_t(clamped(v)*32767); };

			left.record(num_samples,
				[&] (Record::Time_window const tw, Samples_ptr const &samples) {
					depleted = false;

					for (unsigned i = 0; i < SAMPLES_PER_PERIOD; i++)
						data[i*CHANNELS] = float_to_s16(samples.start[i]);

					right.record_at(tw, num_samples,
						[&] (Samples_ptr const &samples) {
							for (unsigned i = 0; i < SAMPLES_PER_PERIOD; i++)
								data[i*CHANNELS + 1] = float_to_s16(samples.start[i]);
						});
				},
				[&] {
					depleted = true;
					clear();
				}
			);
		}
	};

	Recording _recording { };

	genode_audio_packet record_packet()
	{
		_recording.from_record_sessions(_left, _right);
		return _recording.depleted ?
		       genode_audio_packet { nullptr, 0 } :
		       genode_audio_packet { _recording.data, sizeof(_recording.data) };
	}

	Stereo_output(Env &env) : _env(env) { }
};


struct Stereo_input : Noncopyable
{
	static constexpr unsigned SAMPLES_PER_PERIOD = Audio_in::PERIOD;
	static constexpr unsigned CHANNELS = 2;

	Env &_env;

	Play::Connection _left  { _env, "mic_left"  };
	Play::Connection _right { _env, "mic_right" };

	struct Frame { float left, right; };

	void _for_each_frame(genode_audio_packet &packet, auto const &fn) const
	{
		float const scale = 1.0f/32768;

		/* convert two channel s16le interleaved to two channel float */
		for (unsigned i = 0; i < packet.samples; i++)
			fn(Frame { .left  = scale*float(packet.data[i*CHANNELS]),
			           .right = scale*float(packet.data[i*CHANNELS + 1]) });
	}

	Play::Time_window _time_window { };

	void play_packet(genode_audio_packet &packet)
	{
		Play::Duration const duration_us { 10666 }; /* hint for first period (48KHz) */
		_time_window = _left.schedule_and_enqueue(_time_window, duration_us,
			[&] (auto &submit) {
				_for_each_frame(packet, [&] (Frame const frame) {
					submit(frame.left); }); });

		_right.enqueue(_time_window,
			[&] (auto &submit) {
				_for_each_frame(packet, [&] (Frame const frame) {
					submit(frame.right); }); });
	}

	Stereo_input(Env &env) : _env(env) { }
};


struct Audio
{
	Env       &env;
	Allocator &alloc;

	Constructible <Expanding_reporter>  mixer{ };

	Reporter devices { env, "devices" };

	Attached_rom_dataspace config { env, "config" };
	Signal_handler<Audio>  config_handler { env.ep(), *this,
		&Audio::config_update };

	bool const record_play = true;

	bool mixer_update { false };
	Device_mode speaker_mode = Device_mode::DEFAULT;
	Device_mode microphone_mode = Device_mode::DEFAULT;

	Constructible<Audio_out::Out>  out      { };
	Constructible<Audio_out::Root> out_root { };

	Constructible<Audio_in::In>   in      { };
	Constructible<Audio_in::Root> in_root { };

	Constructible<Stereo_output> stereo_output { };
	Constructible<Stereo_input>  stereo_input  { };

	Audio(Env &env, Allocator &alloc)
	: env(env), alloc(alloc)
	{
		config.sigh(config_handler);
		config_update();

		if (record_play) {
			stereo_output.construct(env);
			stereo_input.construct(env);
			return;
		}

		/* Audio_out/Audio_in mode */

		out.construct(env);
		out_root.construct(env, alloc, out->data_avail());
		env.parent().announce(env.ep().manage(*out_root));

		in.construct();
		in_root.construct(env, alloc);
		env.parent().announce(env.ep().manage(*in_root));
	}

	void config_update()
	{
		config.update();

		bool mixer_report = config.node().attribute_value("report_mixer", false);

		if (mixer_report && !mixer.constructed())
			mixer.construct(env, "mixer", "mixer", Expanding_reporter::Initial_buffer_size { 8192 } );
		else if (!mixer_report && mixer.constructed())
			mixer.destruct();

		mixer_update = config.node().has_sub_node("control");

		typedef String<9> Mode;
		Mode speaker    = config.node().attribute_value("speaker_priority", Mode("external"));
		Mode microphone = config.node().attribute_value("mic_priority"    , Mode("external"));

		speaker_mode    = (speaker    == "internal") ? INTERNAL : EXTERNAL;
		microphone_mode = (microphone == "internal") ? INTERNAL : EXTERNAL;

		devices.enabled(config.node().attribute_value("report_devices", false));
	}

	struct genode_audio_packet record_packet()
	{
		if (record_play) return stereo_output->record_packet();

		return _audio_out_active() ? out->record_packet() : genode_audio_packet { nullptr, 0 };
	}

	void play_packet(struct genode_audio_packet &packet)
	{
		if (record_play) stereo_input->play_packet(packet);
		else in->play_packet(packet);
	}
};


Audio &_audio(Audio *audio = nullptr)
{
	static Audio &_audio = *audio;
	return _audio;
}


/**
 * C-interface
 */

extern "C" void genode_audio_init(struct genode_env *env_ptr,
                                  struct genode_allocator *alloc_ptr)
{
	Env *env = static_cast<Env*>(env_ptr);
	Allocator *alloc = static_cast<Allocator*>(alloc_ptr);

	static Audio audio { *env, *alloc };
	_audio(&audio);
}


extern "C" unsigned long genode_audio_samples_per_period(void)
{
	return Audio_out::PERIOD;
}


extern "C" struct genode_audio_packet genode_audio_record(void)
{
	return _audio().record_packet();
}


extern "C" void genode_audio_play(genode_audio_packet packet)
{
	_audio().play_packet(packet);
}


/*
 * mixer report
 */
static void report_bool(Generator &g, genode_mixer_control *control)
{
	for (unsigned i = 0; i < control->value_count; i++) {
		g.node("control", [&]() {
			g.attribute("name",    control->name);
			g.attribute("id",      control->id);
			g.attribute("channel", i);
			g.attribute("type",    control->type_label);
			g.attribute("value",   (bool)control->values[i]);
		});
	}
}


static void report_integer(Generator &g, genode_mixer_control *control)
{
	for (unsigned i = 0; i < control->value_count; i++) {
		g.node("control", [&]() {
			g.attribute("name",    control->name);
			g.attribute("id",      control->id);
			g.attribute("channel", i);
			g.attribute("type",    control->type_label);
			g.attribute("value",   control->values[i]);
			g.attribute("min",     control->min);
			g.attribute("max",     control->max);
		});
	}
}


static void report_enum(Generator &g, genode_mixer_control *control)
{
	g.node("control", [&]() {
		g.attribute("name", control->name);
		g.attribute("id",   control->id);
		g.attribute("type", control->type_label);
		g.attribute("selected", control->values[0]);

		for (unsigned i = 0; i < control->enum_count; i++) {
			g.node("value", [&]() {
				g.attribute("name", control->enum_strings[i]);
				g.attribute("id", i);
			});
		}
	});
};


extern "C" bool genode_mixer_update(void)
{
	return _audio().mixer_update;
}

extern "C" enum Device_mode genode_speaker_mode(void)
{
	return _audio().speaker_mode;
}

extern "C" enum Device_mode genode_microphone_mode(void)
{
	return _audio().microphone_mode;
}


typedef String<5> Type;

static Ctrl_type genode_control_type(Type const &type)
{
	if (type == "bool") return CTRL_BOOL;
	if (type == "int")  return CTRL_INTEGER;
	if (type == "enum") return CTRL_ENUMERATED;
	return CTRL_INVALID;
}


struct genode_mixer_control *mixer_control(struct genode_mixer_controls *controls,
                                           unsigned const id, unsigned const max)
{
	for (unsigned i = 0; i < controls->count; i++) {
		if (controls->control[i].id == id) return &controls->control[i];
	}

	if (controls->count >= max) return nullptr;

	genode_mixer_control *control =  &controls->control[controls->count];
	controls->count++;

	control->values[0] = ~0u;
	control->values[1] = ~0u;

	return control;
}


extern "C" void genode_mixer_update_controls(struct genode_mixer_controls *controls, bool force)
{
	unsigned const max = controls->count;
	controls->count = 0;

	if (!_audio().mixer_update && !force) return;

	using Name = Genode::String<32>;
	Name const config_profile_name =
		_audio().config.node().attribute_value("profile", Name());

	if (config_profile_name == "") {
		warning("no profile name specified");
		return;
	}

	_audio().config.node().for_each_sub_node("profile", [&] (Node const &node) {

		Name const profile_name = node.attribute_value("name", Name());
		if (profile_name != config_profile_name)
			return;

		node.for_each_sub_node("control", [&] (Node const &node) {

			unsigned id = node.attribute_value("id", ~0u);
			if (id == ~0u) return;

			Type const name = node.attribute_value("type", Type());
			Ctrl_type  type = genode_control_type(name);
			if (type == CTRL_INVALID) return;

			unsigned channel = node.attribute_value("channel", ~0u);
			if (type != CTRL_ENUMERATED && channel > 1) return;

			unsigned selected = node.attribute_value("selected", ~0u);
			if (type == CTRL_ENUMERATED && selected == ~0u) return;

			struct genode_mixer_control *control = mixer_control(controls, id, max);
			if (control == nullptr) return;

			control->id   = id;
			control->type = type;

			switch (type) {
			case CTRL_BOOL:
			{
				bool value = node.attribute_value("value", false);
				control->values[channel] = value ? 1 : 0;
				break;
			}
			case CTRL_INTEGER:
			{
				unsigned value = node.attribute_value("value", ~0u);
				control->values[channel] = value;
				break;
			}
			case CTRL_ENUMERATED:
			{
				control->values[0] = selected;
				break;
			}
			case CTRL_INVALID: break;
			}
		});
	});

	_audio().mixer_update = false;
}


extern "C" void genode_mixer_report_controls(struct genode_mixer_controls *controls)
{
	if (!_audio().mixer.constructed()) return;

	Expanding_reporter &mixer = *_audio().mixer;

	mixer.generate([&] (Generator &g) {
		for (unsigned i = 0; i < controls->count; i++) {
			switch (controls->control[i].type) {
			case CTRL_BOOL:       report_bool(g, &controls->control[i]);    break;
			case CTRL_INTEGER:    report_integer(g, &controls->control[i]); break;
			case CTRL_ENUMERATED: report_enum(g, &controls->control[i]);    break;
			default: continue;
			}
		}
	});
}


extern "C" void genode_devices_report(struct genode_devices *devices)
{
	Reporter &reporter = _audio().devices;

	if (reporter.enabled() == false) return;

	Reporter::Result const result =
 		reporter.generate([&](Generator &g) {
			for (unsigned i = 0; i < 64; i++)
				for (unsigned j = 0; j < 2; j++) {
					genode_device *device = &devices->device[i][j];
					if (device->valid == false) continue;
					g.node("device", [&]() {
						g.attribute("number", i);
						g.attribute("direction", device->direction);
						g.attribute("node", device->node);
						g.attribute("name", device->name);
					});
			}
		});

	if (result == Buffer_error::EXCEEDED)
		warning("\"devices\" report exceeds maximum size");
}

extern "C" bool genode_query_routing(struct genode_routing *routing)
{
	if (!routing)
		return false;

	bool result = false;

	using Name = Genode::String<32>;
	Name const config_profile_name =
		_audio().config.node().attribute_value("profile", Name());

	if (config_profile_name == "") {
		warning("no profile name specified");
		return false;
	}

	_audio().config.node().for_each_sub_node("profile", [&] (Node const &node) {

		Name const profile_name = node.attribute_value("name", Name());
		if (profile_name != config_profile_name)
			return;

		node.with_sub_node("routing",
			[&] (Node const &node) {

				log("Use routing information from profile ", profile_name);

				using Device_name = String<16>;

				Device_name const playback =
					node.attribute_value("playback", Device_name("N/A"));
				Device_name const mic_headset =
					node.attribute_value("mic_headset", Device_name("N/A"));
				Device_name const mic_internal =
					node.attribute_value("mic_internal", Device_name("N/A"));

				unsigned const speaker_external_id =
					node.attribute_value("speaker_external_id", 0u);
				unsigned const speaker_internal_id =
					node.attribute_value("speaker_internal_id", 0u);
				unsigned const mic_external_id =
					node.attribute_value("mic_external_id", 0u);
				unsigned const mic_internal_id =
					node.attribute_value("mic_internal_id", 0u);

				memcpy(routing->playback, playback.string(), sizeof(routing->playback));
				memcpy(routing->mic_headset, mic_headset.string(), sizeof(routing->mic_headset));
				memcpy(routing->mic_internal, mic_internal.string(), sizeof(routing->mic_internal));

				routing->speaker_external_index = speaker_external_id;
				routing->speaker_internal_index = speaker_internal_id;
				routing->mic_external_index = mic_external_id;
				routing->mic_internal_index = mic_internal_id;

				result = true;
			},
			[&] { error("no routing specified"); });
	});

	return result;
}
