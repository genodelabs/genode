/*
 * \brief  OSS to Record and Play session translator plugin
 * \author Josef Soentgen
 * \date   2024-02-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/registry.h>
#include <base/signal.h>
#include <os/vfs.h>
#include <play_session/connection.h>
#include <record_session/connection.h>
#include <timer_session/connection.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>
#include <vfs/value_file_system.h>

static constexpr bool VERBOSE = false;

namespace Vfs {
	using namespace Genode;

	struct Oss_file_system;
} /* namespace Vfs */


struct Vfs::Oss_file_system
{
	using Name = String<32>;

	struct Audio;

	struct Data_file_system;
	struct Local_factory;
	struct Compound_file_system;
};


struct Vfs::Oss_file_system::Audio
{
	public:

		struct Info
		{
			unsigned  plugin_version;
			unsigned  channels;
			unsigned  format;
			unsigned  sample_rate;
			unsigned  ifrag_total;
			unsigned  ifrag_size;
			unsigned  ifrag_avail;
			unsigned  ifrag_bytes;
			unsigned  ofrag_total;
			unsigned  ofrag_size;
			unsigned  ofrag_avail;
			unsigned  ofrag_bytes;
			long long optr_samples;
			unsigned  optr_fifo_samples;
			unsigned  play_underruns;

			Readonly_value_file_system<unsigned>  &_channels_fs;
			Readonly_value_file_system<unsigned>  &_format_fs;
			Value_file_system<unsigned>           &_sample_rate_fs;
			Value_file_system<unsigned>           &_ifrag_total_fs;
			Value_file_system<unsigned>           &_ifrag_size_fs;
			Readonly_value_file_system<unsigned>  &_ifrag_avail_fs;
			Readonly_value_file_system<unsigned>  &_ifrag_bytes_fs;
			Value_file_system<unsigned>           &_ofrag_total_fs;
			Value_file_system<unsigned>           &_ofrag_size_fs;
			Readonly_value_file_system<unsigned>  &_ofrag_avail_fs;
			Readonly_value_file_system<unsigned>  &_ofrag_bytes_fs;
			Readonly_value_file_system<long long> &_optr_samples_fs;
			Readonly_value_file_system<unsigned>  &_optr_fifo_samples_fs;
			Value_file_system<unsigned>           &_play_underruns_fs;

			Info(Readonly_value_file_system<unsigned>  &channels_fs,
			     Readonly_value_file_system<unsigned>  &format_fs,
			     Value_file_system<unsigned>           &sample_rate_fs,
			     Value_file_system<unsigned>           &ifrag_total_fs,
			     Value_file_system<unsigned>           &ifrag_size_fs,
			     Readonly_value_file_system<unsigned>  &ifrag_avail_fs,
			     Readonly_value_file_system<unsigned>  &ifrag_bytes_fs,
			     Value_file_system<unsigned>           &ofrag_total_fs,
			     Value_file_system<unsigned>           &ofrag_size_fs,
			     Readonly_value_file_system<unsigned>  &ofrag_avail_fs,
			     Readonly_value_file_system<unsigned>  &ofrag_bytes_fs,
			     Readonly_value_file_system<long long> &optr_samples_fs,
			     Readonly_value_file_system<unsigned>  &optr_fifo_samples_fs,
			     Value_file_system<unsigned>           &play_underruns_fs)
			:
				plugin_version        { 2 },
				channels              { 0 },
				format                { 0 },
				sample_rate           { 0 },
				ifrag_total           { 0 },
				ifrag_size            { 0 },
				ifrag_avail           { 0 },
				ifrag_bytes           { 0 },
				ofrag_total           { 0 },
				ofrag_size            { 0 },
				ofrag_avail           { 0 },
				ofrag_bytes           { 0 },
				optr_samples          { 0 },
				optr_fifo_samples     { 0 },
				play_underruns        { 0 },
				_channels_fs          { channels_fs },
				_format_fs            { format_fs },
				_sample_rate_fs       { sample_rate_fs },
				_ifrag_total_fs       { ifrag_total_fs },
				_ifrag_size_fs        { ifrag_size_fs },
				_ifrag_avail_fs       { ifrag_avail_fs },
				_ifrag_bytes_fs       { ifrag_bytes_fs },
				_ofrag_total_fs       { ofrag_total_fs },
				_ofrag_size_fs        { ofrag_size_fs },
				_ofrag_avail_fs       { ofrag_avail_fs },
				_ofrag_bytes_fs       { ofrag_bytes_fs },
				_optr_samples_fs      { optr_samples_fs },
				_optr_fifo_samples_fs { optr_fifo_samples_fs },
				_play_underruns_fs    { play_underruns_fs }
			{ }

			void update()
			{
				_channels_fs         .value(channels);
				_format_fs           .value(format);
				_sample_rate_fs      .value(sample_rate);
				_ifrag_total_fs      .value(ifrag_total);
				_ifrag_size_fs       .value(ifrag_size);
				_ifrag_avail_fs      .value(ifrag_avail);
				_ifrag_bytes_fs      .value(ifrag_bytes);
				_ofrag_total_fs      .value(ofrag_total);
				_ofrag_size_fs       .value(ofrag_size);
				_ofrag_avail_fs      .value(ofrag_avail);
				_ofrag_bytes_fs      .value(ofrag_bytes);
				_optr_samples_fs     .value(optr_samples);
				_optr_fifo_samples_fs.value(optr_fifo_samples);
				_play_underruns_fs   .value(play_underruns);
			}

			void print(Genode::Output &out) const
			{
				char buf[512] { };

				Genode::Xml_generator xml(buf, sizeof(buf), "oss", [&] () {
					xml.attribute("plugin_version",    plugin_version);
					xml.attribute("channels",          channels);
					xml.attribute("format",            format);
					xml.attribute("sample_rate",       sample_rate);
					xml.attribute("ifrag_total",       ifrag_total);
					xml.attribute("ifrag_size",        ifrag_size);
					xml.attribute("ifrag_avail",       ifrag_avail);
					xml.attribute("ifrag_bytes",       ifrag_bytes);
					xml.attribute("ofrag_total",       ofrag_total);
					xml.attribute("ofrag_size",        ofrag_size);
					xml.attribute("ofrag_avail",       ofrag_avail);
					xml.attribute("ofrag_bytes",       ofrag_bytes);
					xml.attribute("optr_samples",      optr_samples);
					xml.attribute("optr_fifo_samples", optr_fifo_samples);
					xml.attribute("play_underruns",    play_underruns);
				});

				Genode::print(out, Genode::Cstring(buf));
			}
		};

		using Read_result  = Vfs::File_io_service::Read_result;
		using Write_result = Vfs::File_io_service::Write_result;

		/*
		 * Simple sample buffer used for storing play samples given
		 * by the client in float and record samples in int16_t given
		 * by the mixer.
		 *
		 * The used indicator variable is implicitly guarded by
		 * the calling code checking read/write avail beforehand.
		 */
		template <typename T, unsigned SIZE_LOG2>
		struct Sample_buffer_base
		{
			static constexpr unsigned SIZE = 1u << SIZE_LOG2,
			                          MASK = SIZE - 1;
			T _samples[SIZE] { };

			unsigned _rpos = 0;
			unsigned _wpos = 0;
			unsigned _used = 0;

			void reset()
			{
				_rpos = _wpos = _used = 0;
			}

			void insert(T value)
			{
				_used++;
				_wpos = (_wpos + 1) & MASK;
				_samples[_wpos] = value;
			}

			T remove()
			{
				_used--;
				_rpos = (_rpos + 1) & MASK;
				return _samples[_rpos];
			}

			bool read_samples_avail(unsigned const min) const {
				return _used >= min; }

			bool write_samples_avail(unsigned const min) const {
				return SIZE - _used >= min; }

			unsigned used() const { return _used; }

			unsigned used_bytes() const { return _used * sizeof(T); }

			size_t sample_size() const { return sizeof(T); }
		};

		struct Periodic_timer
		{
			Timer::Connection _timer;
			bool              _started;

			Periodic_timer(Genode::Env &env)
			: _timer { env }, _started { false } { }

			void sigh(Signal_context_capability cap) {
				_timer.sigh(cap); }

			void start(unsigned const duration_us)
			{
				_timer.trigger_periodic(duration_us);
				_started = true;
			}

			void stop()
			{
				_timer.trigger_periodic(0);
				_started = false;
			}

			bool started() const { return _started; }
		};

	private:

		Audio(Audio const &);
		Audio &operator = (Audio const &);

		Vfs::Env &_vfs_env;

		Info &_info;
		Readonly_value_file_system<Info, 512> &_info_fs;

		unsigned _frame_size { 0 };

		static unsigned _format_size(unsigned fmt)
		{
			if (fmt == 0x00000010u) /* S16LE */
				return 2u;

			return 0u;
		}

		void _with_duration(size_t const bytes, auto const &fn)
		{
			unsigned const samples      = (unsigned)bytes / _frame_size;
			float    const tmp_duration = float(1'000'000u)
			                            / float(_info.sample_rate)
			                            * float(samples);

			fn(unsigned(tmp_duration), samples);
		}

		/************
		 ** Output **
		 ************/

		struct Stereo_output
		{
			using Sample_buffer = Sample_buffer_base<float, 14>;

			struct Channel { unsigned value; };

			struct Num_samples { unsigned value; };

			static constexpr unsigned    CHANNELS = 2u;
			static constexpr float const SCALE    = 1.0f/32768;

			Genode::Env &_env;

			Constructible<Play::Connection> _session       [CHANNELS] { };
			Sample_buffer                   _session_buffer[CHANNELS] { };
			Periodic_timer                  _timer                    { _env };
			Play::Time_window               _time_window              { };
			bool                            _started                  { false };

			/* runtime parameters */
			Play::Duration _duration       { 0 };
			Num_samples    _samples        { 0 };
			unsigned       _underrun_limit { 0 };

			void _for_each_sample(Sample_buffer     &buffer,
			                      Num_samples const  samples,
			                      auto        const &fn)
			{
				for (unsigned i = 0; i < samples.value; i++)
					fn(buffer.remove());
			}

			void _for_each_session(auto const &fn)
			{
				for (auto & session : _session)
					if (session.constructed())
						fn(*session);
			}

			void _create_sessions(Genode::Env &env)
			{
				/* for now force stereo only */
				if (CHANNELS != 2) {
					struct Unsupported_channel_number : Genode::Exception { };
					throw Unsupported_channel_number();
				}

				char const * const channel_map[CHANNELS] = { "left", "right" };

				for (unsigned i = 0; i < CHANNELS; i++)
					_session[i].construct(env, channel_map[i]);
			}

			void _destroy_sessions()
			{
				for (unsigned i = 0; i < CHANNELS; i++)
					_session[i].destruct();
			}

			Stereo_output(Genode::Env &env) : _env { env } { }

			void update_parameters(Play::Duration const duration,
			                       Num_samples    const samples)
			{
				_duration = duration;
				_samples  = samples;

				_underrun_limit = 1'000'000u / _duration.us;
			}

			void schedule_and_enqueue()
			{
				bool first = true;

				if (!_session[0].constructed())
					_create_sessions(_env);

				unsigned buffer_idx = 0;
				_for_each_session([&] (Play::Connection & session) {
					if (first) {
						_time_window = session.schedule_and_enqueue(_time_window, _duration,
							[&] (auto &submit) {
								_for_each_sample(_session_buffer[buffer_idx], _samples,
									[&] (float const v) { submit(v); }); });
						first = false;
					} else {
						session.enqueue(_time_window,
							[&] (auto &submit) {
								_for_each_sample(_session_buffer[buffer_idx], _samples,
									[&] (float const v) { submit(v); }); });
					}

					++buffer_idx;
				});
			}

			void consume(Channel              const  channel,
			             Const_byte_range_ptr const &src,
			             Num_samples          const  src_samples)
			{
				int16_t const *data = reinterpret_cast<int16_t const*>(src.start);

				for (unsigned i = 0; i < src_samples.value; i++) {
					float const v = SCALE * float(data[i * CHANNELS + channel.value]);
					_session_buffer[channel.value].insert(v);
				}
			}

			void halt()
			{
				_timer.stop();

				_for_each_session([&] (Play::Connection & session) {
					session.stop();
				});

				_destroy_sessions();

				for (auto & buffer : _session_buffer)
					buffer.reset();

				_time_window = Play::Time_window { };
			}

			void timer_sigh(Signal_context_capability cap) {
				_timer.sigh(cap); }

			void timer_start() {
				_timer.start(_duration.us); }

			bool timer_started() const {
				return _timer.started(); }

			void play_started(bool start) {
				_started = start; }

			bool play_started() const {
				return _started; }

			bool samples_avail(unsigned const samples) const {
				return _session_buffer[0].read_samples_avail(samples); }

			bool space_avail(unsigned const samples) const {
				return _session_buffer[0].write_samples_avail(samples); }

			unsigned samples_per_channel() const { return _samples.value; }

			unsigned underrun_limit() const { return _underrun_limit; }
		};

		Constructible<Stereo_output> _stereo_output { };

		void _with_stereo_output(auto const &fn)
		{
			if (_stereo_output.constructed())
				fn(*_stereo_output);
		}

		void _with_stereo_output(auto const &fn) const
		{
			if (_stereo_output.constructed())
				fn(*_stereo_output);
		}

		bool _try_schedule_and_enqueue(Stereo_output &output)
		{
			if (!output.samples_avail(output.samples_per_channel()))
				return false;

			output.play_started(true);

			if (!output.timer_started())
				output.timer_start();

			output.schedule_and_enqueue();

			/*
			 * For now we ignore 'optr_samples' altogether but we
			 * could use it later on to denote the samples currently
			 * played while 'optr_fifo_samples' sums up the samples
			 * in the ring-buffer.
			 *
			 * XXX optr_fifo_samples currently represents optr_samples
			 */
			_info.optr_fifo_samples += output.samples_per_channel();
			_update_output_info();

			return true;
		}

		void _try_starting_schedule_and_enqueue(Stereo_output &output)
		{
			if (!output.play_started())
				(void)_try_schedule_and_enqueue(output);
		}

		void _halt_output(Stereo_output &output)
		{
			output.halt();
			output.play_started(false);
		}

		void _update_output_info()
		{
			_info.ofrag_bytes = unsigned((_info.ofrag_total * _info.ofrag_size)
			                  - (_info.optr_fifo_samples * _frame_size));
			_info.ofrag_avail = _info.ofrag_bytes / _info.ofrag_size;

			_info.update();
			_info_fs.value(_info);
		}

		/***********
		 ** Input **
		 ***********/

		struct Stereo_input
		{
			using Sample_buffer = Sample_buffer_base<int16_t, 14>;

			static constexpr unsigned CHANNELS = 2u;

			Genode::Env &_env;

			struct Duration { unsigned us; };

			Constructible<Record::Connection> _session       [CHANNELS] { };
			Sample_buffer                     _session_buffer[CHANNELS] { };
			Periodic_timer                    _timer                    { _env };

			/* runtime parameters */
			Duration            _timer_duration { 0 };
			Record::Num_samples _num_samples    { 0 };

			void _for_each_record_session(auto const &fn)
			{
				for (auto & session : _session)
					if (session.constructed())
						fn(*session);
			}

			void _create_sessions(Genode::Env &env)
			{
				/* for now force stereo input */
				if (CHANNELS != 2) {
					struct Unsupported_channel_number : Genode::Exception { };
					throw Unsupported_channel_number();
				}

				char const * const channel_map[CHANNELS] = { "left", "right" };

				for (unsigned i = 0; i < CHANNELS; i++)
					_session[i].construct(env, channel_map[i]);
			}

			void _destroy_sessions()
			{
				for (unsigned i = 0; i < CHANNELS; i++)
					_session[i].destruct();
			}

			Stereo_input(Genode::Env &env) : _env { env } { }

			void update_parameters(Duration            const duration,
			                       Record::Num_samples const num_samples)
			{
				_timer_duration = duration;
				_num_samples    = num_samples;
			}

			void halt()
			{
				_timer.stop();

				for (auto & buffer : _session_buffer)
					buffer.reset();

				_destroy_sessions();
			}

			enum class Record_result { RECORD_OK, RECORD_UNDERRUN, RECORD_OVERRUN };

			Record_result record()
			{
				if (!_session[0].constructed())
					_create_sessions(_env);

				if (!_session_buffer[0].write_samples_avail(_num_samples.value()))
					return Record_result::RECORD_OVERRUN;

				auto clamped = [&] (float v)
				{
					return (v >  1.0) ?  1.0
					     : (v < -1.0) ? -1.0
					     :  v;
				};

				auto float_to_s16 = [&] (float v) { return int16_t(clamped(v)*32767); };

				bool depleted = false;
				_session[0]->record(_num_samples,
					[&] (Record::Time_window const tw,
						 Record::Connection::Samples_ptr const &samples) {
							for (unsigned i = 0; i < _num_samples.value(); i++)
								_session_buffer[0].insert(float_to_s16(samples.start[i]));

						_session[1]->record_at(tw, _num_samples,
							[&] (Record::Connection::Samples_ptr const &samples) {
								for (unsigned i = 0; i < _num_samples.value(); i++)
									_session_buffer[1].insert(float_to_s16(samples.start[i]));
							});
					},
					[&] { depleted = true; });

				return depleted ? Record_result::RECORD_UNDERRUN : Record_result::RECORD_OK;
			}

			size_t produce(Byte_range_ptr const &dst, size_t const length)
			{
				unsigned const samples = unsigned(length
				                       / (CHANNELS * _session_buffer[0].sample_size()));

				int16_t *data = reinterpret_cast<int16_t*>(dst.start);
				for (unsigned i = 0; i < samples; i++) {
					data[i*CHANNELS+0] = _session_buffer[0].remove();
					data[i*CHANNELS+1] = _session_buffer[1].remove();
				}

				return length;
			}

			void timer_sigh(Signal_context_capability cap) {
				_timer.sigh(cap); }

			void timer_start() {
				_timer.start(_timer_duration.us); }

			bool timer_started() const {
				return _timer.started(); }

			unsigned bytes_avail() const { return _session_buffer[0].used_bytes() * CHANNELS; }
		};

		Constructible<Stereo_input> _stereo_input { };

		void _with_input(auto const &fn)
		{
			if (_stereo_input.constructed())
				fn(*_stereo_input);
		}

		void _with_input(auto const &fn) const
		{
			if (_stereo_input.constructed())
				fn(*_stereo_input);
		}

		void _try_record(Stereo_input &input)
		{
			if (!input.timer_started())
				input.timer_start();

			using Record_result = Stereo_input::Record_result;

			Record_result const result = input.record();
			switch (result) {
			case Record_result::RECORD_OK:
				break;
			case Record_result::RECORD_UNDERRUN:
				warning("underrun while recording");
				break;
			case Record_result::RECORD_OVERRUN:
				warning("overrun while recording");
				input.halt();
				break;
			}

			_info.ifrag_bytes = input.bytes_avail();
			_update_input_info();
		}

		void _halt_input(Stereo_input &input)
		{
			_info.ifrag_bytes = 0;
			input.halt();
		}

		void _update_input_info()
		{
			_info.ifrag_avail = _info.ifrag_bytes / _info.ifrag_size;

			_info.update();
			_info_fs.value(_info);
		}

		struct Config
		{
			enum : unsigned {

				FRAGS_TOTAL  = 4u,
				FRAGS_QUEUED = FRAGS_TOTAL / 2,

				/*  512 S16LE stereo -> 11.6 ms at 44.1kHz */
				MIN_OFRAG_SIZE = 2048u,
				/* 2048 S16LE stereo -> 46.4 ms at 44.1kHz */
				MAX_OFRAG_SIZE = 8192u,

				MIN_IFRAG_SIZE = MIN_OFRAG_SIZE,
				MAX_IFRAG_SIZE = MAX_OFRAG_SIZE,

				/* cover lower input rates (e.g. voice recordings) */
				MIN_SAMPLE_RATE =  8'000u,
				/* limit max to reasonable playback rates */
				MAX_SAMPLE_RATE = 48'000u,
			};

			bool     verbose;

			unsigned frags_total;
			unsigned frags_queued;

			bool     play_enabled;
			unsigned max_ofrag_size;
			unsigned min_ofrag_size;

			bool     record_enabled;
			unsigned max_ifrag_size;
			unsigned min_ifrag_size;

			unsigned max_sample_rate;
			unsigned min_sample_rate;

			void print(Genode::Output &out) const
			{
				Genode::print(out, "verbose: ",         verbose,         " "
				                   "play_enabled: ",    play_enabled,    " "
				                   "min_ofrag_size: ",  min_ofrag_size,  " "
				                   "max_ofrag_size: ",  max_ofrag_size,  " "
				                   "record_enabled: ",  record_enabled,  " "
				                   "min_ifrag_size: ",  min_ifrag_size,  " "
				                   "max_ifrag_size: ",  max_ifrag_size,  " "
				                   "min_sample_rate: ", min_sample_rate, " "
				                   "max_sample_rate: ", max_sample_rate, " "
				);
			}

			static Config from_xml(Xml_node const &);
		};

		Config const _config;

	public:

		Audio(Vfs::Env                              &env,
		      Info                                  &info,
		      Readonly_value_file_system<Info, 512> &info_fs,
		      Xml_node                               config)
		:
			_vfs_env { env },
			_info    { info },
			_info_fs { info_fs },
			_config  { Config::from_xml(config) }
		{
			log("OSS: ", _config);

			/* hard-code initial values for now */
			_info.channels    = 2u;
			_info.format      = (unsigned)0x00000010; /* S16LE */
			_info.sample_rate = 44'100u;

			_frame_size = _info.channels * _format_size(_info.format);

			if (_config.play_enabled) {
				_stereo_output.construct(_vfs_env.env());

				_info.ofrag_size  = _config.min_ofrag_size;
				_info.ofrag_total = _config.frags_total;
				_info.ofrag_avail = _info.ofrag_total;
				_info.ofrag_bytes = _info.ofrag_avail * _info.ofrag_size;

				update_output_duration(_info.ofrag_size);
			}

			if (_config.record_enabled) {
				_stereo_input.construct(_vfs_env.env());

				_info.ifrag_size  = _config.min_ifrag_size;
				_info.ifrag_total = _config.frags_total;
				_info.ifrag_avail = 0;
				_info.ifrag_bytes = 0;

				update_input_duration(_info.ifrag_size);
			}

			_info.update();
			_info_fs.value(_info);
		}

		bool verbose() const { return _config.verbose; }

		unsigned frags_total() const { return _config.frags_total; }

		/********************
		 ** Record session **
		 ********************/

		unsigned max_ifrag_size() const { return _config.max_ifrag_size; }

		unsigned min_ifrag_size() const { return _config.min_ifrag_size; }

		void update_input_duration(unsigned const bytes)
		{
			_with_input([&] (Stereo_input &input) {
				_with_duration(bytes, [&] (unsigned const duration, unsigned const samples) {
					input.update_parameters(Stereo_input::Duration { duration },
					                        Record::Num_samples    { samples });
				});
			});
		}

		void record_timer_sigh(Signal_context_capability cap)
		{
			_with_input([&] (Stereo_input &input) {
				input.timer_sigh(cap); });
		}

		bool handle_record_timer()
		{
			bool result = false;
			_with_input([&] (Stereo_input &input) {
				_try_record(input);
				result = true;
			});
			return result;
		}

		void enable_input(bool enable)
		{
			if (_config.verbose)
				log(__func__, ": ", enable ? "on" : "off");

			_with_input([&] (Stereo_input &input) {
				if (enable == false) _halt_input(input);
				else                 _try_record(input);
			});
		}

		bool read_ready() const
		{
			if (!_config.record_enabled)
				return false;

			bool result = false;
			_with_input([&] (Stereo_input const &input) {
				result = input.bytes_avail() >= _info.ifrag_size;
			});
			return result;
		}

		Read_result read(Byte_range_ptr const &dst, size_t &out_size)
		{
			if (!_config.record_enabled)
				return Read_result::READ_ERR_INVALID;

			Read_result result = Read_result::READ_ERR_IO;
			_with_input([&] (Stereo_input &input) {

				/* get the ball rolling on first read */
				if (!input.timer_started())
					_try_record(input);

				/*
				 * Wait until we have at least on input fragment
				 * available
				 */

				unsigned const avail = input.bytes_avail();
				if (avail < _info.ifrag_size) {
					result = Read_result::READ_QUEUED;
					return;
				}

				size_t const length = min((size_t)_info.ifrag_size,
				                          dst.num_bytes);
				out_size = input.produce(dst, length);

				_info.ifrag_bytes = input.bytes_avail();
				_update_input_info();
				result = Read_result::READ_OK;
			});
			return result;
		}

		unsigned max_sample_rate() const { return _config.max_sample_rate; }

		unsigned min_sample_rate() const { return _config.min_sample_rate; }

		/******************
		 ** Play session **
		 ******************/

		unsigned max_ofrag_size() const { return _config.max_ofrag_size; }

		unsigned min_ofrag_size() const { return _config.min_ofrag_size; }

		void update_output_duration(unsigned const bytes)
		{
			_with_stereo_output([&] (Stereo_output &output) {
				_with_duration(bytes, [&] (unsigned const duration, unsigned const samples) {
					output.update_parameters(Play::Duration             { duration },
					                         Stereo_output::Num_samples { samples });
				});
			});
		}

		void play_timer_sigh(Signal_context_capability cap) {
			_stereo_output->timer_sigh(cap); }

		bool handle_play_timer()
		{
			bool enqueued = false;
			_with_stereo_output([&] (Stereo_output &output) {

				/*
				 * We may encountered an underrun when the timer triggered
				 * the last time. At this point fifo samples has already
				 * been zero, only decrement it when something was played.
				 */
				if (_info.optr_fifo_samples) {
					_info.optr_fifo_samples -= output.samples_per_channel();
					_update_output_info();
				}

				enqueued = _try_schedule_and_enqueue(output);
				if (!enqueued) {

					_info.play_underruns++;

					if (_info.play_underruns >= output.underrun_limit()) {
						warning("hit underrun limit (", output.underrun_limit(),
						        ") - stopping playback");
						_halt_output(output);
						_info.play_underruns = 0;
					}
				}
			});
			return enqueued;
		}

		void enable_output(bool enable)
		{
			if (_config.verbose)
				log(__func__, ": ", enable ? "on" : "off");

			_with_stereo_output([&] (Stereo_output &output) {
				if (enable == false) _halt_output(output);
				else                 _try_starting_schedule_and_enqueue(output);
			});
		}

		bool write_ready() const
		{
			bool result = false;
			_with_stereo_output([&] (Stereo_output const &output) {
				unsigned const samples_per_channel = output.samples_per_channel();
				result = output.space_avail  (  samples_per_channel)
				     && !output.samples_avail(_config.frags_queued * samples_per_channel);
			});
			return result;
		}

		Write_result write(Const_byte_range_ptr const &src, size_t &out_size)
		{
			out_size = 0;

			auto sample_count = [&] (Const_byte_range_ptr const &range) {
				return (unsigned)range.num_bytes / _frame_size; };

			unsigned const samples = sample_count(src);

			Write_result result = Write_result::WRITE_ERR_IO;

			_with_stereo_output([&] (Stereo_output &output) {

				/* treat a full buffer and enough buffered in the same way */
				if (!output.space_avail(samples)) {
					result = Write_result::WRITE_ERR_WOULD_BLOCK;
					return;
				}

				if (output.samples_avail(_config.frags_queued * output.samples_per_channel())) {
					result = Write_result::WRITE_ERR_WOULD_BLOCK;
					return;
				}

				for (unsigned i = 0; i < _info.channels; i++)
					output.consume(Stereo_output::Channel { i }, src,
					               Stereo_output::Num_samples { samples });

				/*
				 * Kick-off playback at the first complete fragment, afterwards
				 * this is a NOP as the periodic timer handles further
				 * scheduling.
				 */
				_try_starting_schedule_and_enqueue(output);

				out_size = src.num_bytes;
				result = Write_result::WRITE_OK;
			});

			return result;
		}
};


Vfs::Oss_file_system::Audio::Config
Vfs::Oss_file_system::Audio::Config::from_xml(Xml_node const &config)
{
	auto default_size = [&] (Xml_node     const &config,
	                         char const * const  attr,
	                         unsigned     const  value) {
			return config.attribute_value(attr, value); };

	auto cap_max = [&] (Xml_node     const &config,
	                    char const * const  attr,
	                    unsigned     const  default_value) {
		return min(default_size(config, attr, default_value),
		           default_value); };

	auto cap_min = [&] (Xml_node     const &config,
	                    char const * const  attr,
	                    unsigned     const  default_value) {
		return max(default_size(config, attr, default_value),
		           default_value); };

	auto limit = [&] (unsigned const value, unsigned const max_value) {
		return value > max_value ? max_value : value; };

	/* constrain frag sizes to [min, max] */
	return {
		.verbose = config.attribute_value("verbose", VERBOSE),

		/* hard-coded for now */
		.frags_total  = FRAGS_TOTAL,
		.frags_queued = FRAGS_QUEUED,

		.play_enabled   = config.attribute_value("play_enabled", true),
		.max_ofrag_size = cap_max(config,        "max_ofrag_size", MAX_OFRAG_SIZE),
		.min_ofrag_size = limit(cap_min(config,  "min_ofrag_size", MIN_OFRAG_SIZE),
		                        MAX_OFRAG_SIZE),

		.record_enabled = config.attribute_value("record_enabled", true),
		.max_ifrag_size = cap_max(config,        "max_ifrag_size", MAX_IFRAG_SIZE),
		.min_ifrag_size = limit(cap_min(config,  "min_ifrag_size", MIN_IFRAG_SIZE),
		                        MAX_IFRAG_SIZE),

		.max_sample_rate = cap_max(config,        "max_sample_rate", MAX_SAMPLE_RATE),
		.min_sample_rate = limit(cap_min(config,  "min_sample_rate", MIN_SAMPLE_RATE),
		                        MAX_SAMPLE_RATE)
	};
}


class Vfs::Oss_file_system::Data_file_system : public Single_file_system
{
	private:

		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Entrypoint         &_ep;
		Vfs::Env::User     &_vfs_user;
		Audio              &_audio;

		struct Oss_vfs_handle : public Single_vfs_handle
		{
			Audio &_audio;

			bool _rd_or_rdwr() const
			{
				return status_flags() == STATUS_RDONLY
				    || status_flags() == STATUS_RDWR;
			}

			bool _wr_or_rdwr() const
			{
				return status_flags() == STATUS_WRONLY
				    || status_flags() == STATUS_RDWR;
			}

			Oss_vfs_handle(Directory_service &ds,
			               File_io_service   &fs,
			               Allocator         &alloc,
			               Audio             &audio,
			               int                flags)
			:
				Single_vfs_handle { ds, fs, alloc, flags },
				_audio { audio }
			{ }

			~Oss_vfs_handle()
			{
				if (_rd_or_rdwr())
					_audio.enable_input(false);

				if (_wr_or_rdwr())
					_audio.enable_output(false);
			}

			Read_result read(Byte_range_ptr const &dst,
			                 size_t               &out_count) override {
				return _audio.read(dst, out_count); }

			Write_result write(Const_byte_range_ptr const &src,
			                   size_t                     &out_count) override {
				return _audio.write(src, out_count); }

			bool read_ready() const override {
				return _audio.read_ready(); }

			bool write_ready() const override {
				return _audio.write_ready(); }
		};

		using Registered_handle = Genode::Registered<Oss_vfs_handle>;
		using Handle_registry   = Genode::Registry<Registered_handle>;

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _play_timer {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_play_timer };

		void _handle_play_timer()
		{
			if (_audio.handle_play_timer())
				_vfs_user.wakeup_vfs_user();
		}

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _record_timer {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_record_timer };

		void _handle_record_timer()
		{
			if (_audio.handle_record_timer())
				_vfs_user.wakeup_vfs_user();
		}

	public:

		Data_file_system(Genode::Entrypoint &ep,
		                 Vfs::Env::User     &vfs_user,
		                 Audio              &audio,
		                 Name         const &name)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     Node_rwx::ro(), Genode::Xml_node("<data/>") },

			_ep       { ep },
			_vfs_user { vfs_user },
			_audio    { audio }
		{
			_audio.play_timer_sigh(_play_timer);
			_audio.record_timer_sigh(_record_timer);
		}

		static const char *name()   { return "data"; }
		char const *type() override { return "data"; }

		/*********************************
		 ** Directory service interface **
		 *********************************/

		Open_result open(char const  *path, unsigned flags,
		                 Vfs_handle **out_handle,
		                 Allocator   &alloc) override
		{
			if (!_single_file(path)) {
				return OPEN_ERR_UNACCESSIBLE;
			}

			try {
				*out_handle = new (alloc)
					Registered_handle(_handle_registry,
					                  *this, *this,
					                  alloc, _audio, flags);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override {
			return FTRUNCATE_OK; }
};


struct Vfs::Oss_file_system::Local_factory : File_system_factory
{
	using Label = Genode::String<64>;
	Label const _label;
	Name  const _name;

	Vfs::Env &_env;

	/* RO/RW files */
	Readonly_value_file_system<unsigned>  _channels_fs          { "channels", 0U };
	Readonly_value_file_system<unsigned>  _format_fs            { "format", 0U };
	Value_file_system<unsigned>           _sample_rate_fs       { "sample_rate", 0U };
	Value_file_system<unsigned>           _ifrag_total_fs       { "ifrag_total", 0U };
	Value_file_system<unsigned>           _ifrag_size_fs        { "ifrag_size", 0U} ;
	Readonly_value_file_system<unsigned>  _ifrag_avail_fs       { "ifrag_avail", 0U };
	Readonly_value_file_system<unsigned>  _ifrag_bytes_fs       { "ifrag_bytes", 0U };
	Value_file_system<unsigned>           _ofrag_total_fs       { "ofrag_total", 0U };
	Value_file_system<unsigned>           _ofrag_size_fs        { "ofrag_size", 0U} ;
	Readonly_value_file_system<unsigned>  _ofrag_avail_fs       { "ofrag_avail", 0U };
	Readonly_value_file_system<unsigned>  _ofrag_bytes_fs       { "ofrag_bytes", 0U };
	Readonly_value_file_system<long long> _optr_samples_fs      { "optr_samples", 0LL };
	Readonly_value_file_system<unsigned>  _optr_fifo_samples_fs { "optr_fifo_samples", 0U };
	Value_file_system<unsigned>           _play_underruns_fs    { "play_underruns", 0U };
	Value_file_system<unsigned>           _enable_input_fs      { "enable_input", 1U };
	Value_file_system<unsigned>           _enable_output_fs     { "enable_output", 1U };

	/* WO files */
	Value_file_system<unsigned>           _halt_input_fs        { "halt_input", 0U };
	Value_file_system<unsigned>           _halt_output_fs       { "halt_output", 0U };

	Audio::Info _info { _channels_fs, _format_fs, _sample_rate_fs,
	                    _ifrag_total_fs, _ifrag_size_fs,
	                    _ifrag_avail_fs, _ifrag_bytes_fs,
	                    _ofrag_total_fs, _ofrag_size_fs,
	                    _ofrag_avail_fs, _ofrag_bytes_fs,
	                    _optr_samples_fs, _optr_fifo_samples_fs,
	                    _play_underruns_fs };

	Readonly_value_file_system<Audio::Info, 512> _info_fs { "info", _info };

	Audio _audio;

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_input_handler {
		_enable_input_fs, "/enable_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_input_handler {
		_halt_input_fs, "/halt_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ifrag_total_handler {
		_ifrag_total_fs, "/ifrag_total",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ifrag_total_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ifrag_size_handler {
		_ifrag_size_fs, "/ifrag_size",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ifrag_size_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_output_handler {
		_enable_output_fs, "/enable_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_output_handler {
		_halt_output_fs, "/halt_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_total_handler {
		_ofrag_total_fs, "/ofrag_total",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_total_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_size_handler {
		_ofrag_size_fs, "/ofrag_size",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_size_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _play_underruns_handler {
		_play_underruns_fs, "/play_underruns",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_play_underruns_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _sample_rate_handler {
		_sample_rate_fs, "/sample_rate",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_sample_rate_changed };

	/********************
	 ** Watch handlers **
	 ********************/

	void _enable_input_changed()
	{
		bool const enable = (bool)_enable_input_fs.value();
		_audio.enable_input(enable);
	}

	void _halt_input_changed()
	{
		bool const halt = (bool)_halt_input_fs.value();
		if (halt)
			_audio.enable_input(false);
	}

	void _ifrag_total_changed()
	{
		/*
		 * NOP for now as it is set in tandem with ifrag_size
		 * that in return limits number of fragments.
		 */
	}

	void _ifrag_size_changed()
	{
		/*
		 * Should only be changed while input is currently disabled.
		 */

		unsigned const ifrag_size_max = _audio.max_ifrag_size();
		unsigned const ifrag_size_min = _audio.min_ifrag_size();

		unsigned ifrag_size_new = _ifrag_size_fs.value();

		ifrag_size_new = max(ifrag_size_new, ifrag_size_min);
		ifrag_size_new = min(ifrag_size_new, ifrag_size_max);

		_info.ifrag_size = ifrag_size_new;

		_info.ifrag_total = _audio.frags_total();
		_info.ifrag_avail = 0;
		_info.ifrag_bytes = _info.ifrag_avail * _info.ifrag_size;

		_audio.update_input_duration(_info.ifrag_size);

		_info.update();
		_info_fs.value(_info);

		if (_audio.verbose())
			log("Input fragment size changed to ", _info.ifrag_size);
	}

	void _enable_output_changed()
	{
		bool const enable = (bool)_enable_output_fs.value();
		_audio.enable_output(enable);
	}

	void _halt_output_changed()
	{
		bool const halt = (bool)_halt_output_fs.value();
		if (halt)
			_audio.enable_output(false);
	}

	void _ofrag_total_changed()
	{
		/*
		 * NOP for now as it is set in tandem with ofrag_size
		 * that in return limits number of fragments.
		 */
	}

	void _ofrag_size_changed()
	{
		/*
		 * Should only be changed while output is currently disabled.
		 */

		unsigned const ofrag_size_max = _audio.max_ofrag_size();
		unsigned const ofrag_size_min = _audio.min_ofrag_size();

		unsigned ofrag_size_new = _ofrag_size_fs.value();

		ofrag_size_new = max(ofrag_size_new, ofrag_size_min);
		ofrag_size_new = min(ofrag_size_new, ofrag_size_max);

		_info.ofrag_size = ofrag_size_new;

		_info.ofrag_total = _audio.frags_total();

		_info.ofrag_avail = _info.ofrag_total;
		_info.ofrag_bytes = _info.ofrag_total * _info.ofrag_size;

		_audio.update_output_duration(_info.ofrag_size);

		_info.update();
		_info_fs.value(_info);

		if (_audio.verbose())
			log("Output fragment size changed to ", _info.ofrag_size);
	}

	void _play_underruns_changed()
	{
		/* reset counter */
		_info.play_underruns = 0;

		_info.update();
		_info_fs.value(_info);
	}

	void _sample_rate_changed()
	{
		unsigned const sample_rate_max = _audio.max_sample_rate();
		unsigned const sample_rate_min = _audio.min_sample_rate();

		unsigned sample_rate_new = _sample_rate_fs.value();

		sample_rate_new = max(sample_rate_new, sample_rate_min);
		sample_rate_new = min(sample_rate_new, sample_rate_max);

		_info.sample_rate = sample_rate_new;

		_audio.update_output_duration(_info.ofrag_size);

		_info.update();
		_info_fs.value(_info);

		if (_audio.verbose())
			log("Sample rate changed to ", _info.sample_rate);
	}

	static Name name(Xml_node config)
	{
		return config.attribute_value("name", Name("oss"));
	}

	Data_file_system _data_fs;

	Local_factory(Vfs::Env &env, Xml_node config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_audio  { _env, _info, _info_fs, config },
		_data_fs { _env.env().ep(), env.user(), _audio, name(config) }
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type("data")) return &_data_fs;
		if (node.has_type("info")) return &_info_fs;

		if (node.has_type(Readonly_value_file_system<unsigned>::type_name())) {

			if (_channels_fs.matches(node))          return &_channels_fs;
			if (_ifrag_avail_fs.matches(node))       return &_ifrag_avail_fs;
			if (_ifrag_bytes_fs.matches(node))       return &_ifrag_bytes_fs;
			if (_ofrag_avail_fs.matches(node))       return &_ofrag_avail_fs;
			if (_ofrag_bytes_fs.matches(node))       return &_ofrag_bytes_fs;
			if (_format_fs.matches(node))            return &_format_fs;
			if (_optr_samples_fs.matches(node))      return &_optr_samples_fs;
			if (_optr_fifo_samples_fs.matches(node)) return &_optr_fifo_samples_fs;
		}

		if (node.has_type(Value_file_system<unsigned>::type_name())) {

			if (_enable_input_fs.matches(node))   return &_enable_input_fs;
			if (_enable_output_fs.matches(node))  return &_enable_output_fs;
			if (_halt_input_fs.matches(node))     return &_halt_input_fs;
			if (_halt_output_fs.matches(node))    return &_halt_output_fs;
			if (_ifrag_total_fs.matches(node))    return &_ifrag_total_fs;
			if (_ifrag_size_fs.matches(node))     return &_ifrag_size_fs;
			if (_ofrag_total_fs.matches(node))    return &_ofrag_total_fs;
			if (_ofrag_size_fs.matches(node))     return &_ofrag_size_fs;
			if (_play_underruns_fs.matches(node)) return &_play_underruns_fs;
			if (_sample_rate_fs.matches(node))    return &_sample_rate_fs;
		}

		return nullptr;
	}
};


class Vfs::Oss_file_system::Compound_file_system : private Local_factory,
                                                   public  Vfs::Dir_file_system
{
	private:

		using Name = Oss_file_system::Name;

		using Config = String<1024>;
		static Config _config(Name const &name)
		{
			char buf[Config::capacity()] { };

			/*
			 * By not using the node type "dir", we operate the
			 * 'Dir_file_system' in root mode, allowing multiple sibling nodes
			 * to be present at the mount point.
			 */
			Genode::Xml_generator xml(buf, sizeof(buf), "compound", [&] () {

				xml.node("data", [&] () {
					xml.attribute("name", name); });

				xml.node("dir", [&] () {
					xml.attribute("name", Name(".", name));
					xml.node("info", [&] () { });

					xml.node("readonly_value", [&] {
						xml.attribute("name", "channels");
					});

					xml.node("value", [&] {
							 xml.attribute("name", "sample_rate");
					});

					xml.node("readonly_value", [&] {
						xml.attribute("name", "format");
					});

					xml.node("value", [&] {
						xml.attribute("name", "enable_input");
					});

					xml.node("value", [&] {
						xml.attribute("name", "enable_output");
					});

					xml.node("value", [&] {
						xml.attribute("name", "halt_input");
					});

					xml.node("value", [&] {
						xml.attribute("name", "halt_output");
					});

					xml.node("value", [&] {
						xml.attribute("name", "ifrag_total");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "ifrag_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ifrag_avail");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ifrag_bytes");
					});

					xml.node("value", [&] {
						xml.attribute("name", "ofrag_total");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "ofrag_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ofrag_avail");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "ofrag_bytes");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "optr_samples");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "optr_fifo_samples");
					});

					xml.node("value", [&] {
						 xml.attribute("name", "play_underruns");
					});
				});
			});

			return Config(Genode::Cstring(buf));
		}

	public:

		Compound_file_system(Vfs::Env &vfs_env, Genode::Xml_node node)
		:
			Local_factory { vfs_env, node },
			Vfs::Dir_file_system { vfs_env,
			                       Xml_node(_config(Local_factory::name(node)).string()),
			                       *this }
		{ }

		static const char *name() { return "oss_next"; }

		char const *type() override { return name(); }
};


extern "C" Vfs::File_system_factory *vfs_file_system_factory(void)
{
	struct Factory : Vfs::File_system_factory
	{
		Vfs::File_system *create(Vfs::Env &env, Genode::Xml_node config) override
		{
			return new (env.alloc())
				Vfs::Oss_file_system::Compound_file_system(env, config);
		}
	};

	static Factory f;
	return &f;
}
