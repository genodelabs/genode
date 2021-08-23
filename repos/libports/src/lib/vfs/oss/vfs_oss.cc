/*
 * \brief  OSS emulation to Audio_out file system
 * \author Josef Soentgen
 * \date   2018-10-25
 */

/*
 * Copyright (C) 2018-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_out_session/connection.h>
#include <base/registry.h>
#include <base/signal.h>
#include <os/vfs.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>
#include <vfs/value_file_system.h>

/* libc includes */
#include <sys/soundcard.h>


static constexpr size_t _stream_packet_size { Audio_out::PERIOD * Audio_out::SAMPLE_SIZE };


namespace Vfs { struct Oss_file_system; }


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
			unsigned  channels;
			unsigned  format;
			unsigned  sample_rate;
			unsigned  ofrag_total;
			unsigned  ofrag_size;
			unsigned  ofrag_avail;
			long long optr_samples;
			unsigned  optr_fifo_samples;
			unsigned  play_underruns;

			Readonly_value_file_system<unsigned>  &_channels_fs;
			Readonly_value_file_system<unsigned>  &_format_fs;
			Readonly_value_file_system<unsigned>  &_sample_rate_fs;
			Value_file_system<unsigned>           &_ofrag_total_fs;
			Value_file_system<unsigned>           &_ofrag_size_fs;
			Readonly_value_file_system<unsigned>  &_ofrag_avail_fs;
			Readonly_value_file_system<long long> &_optr_samples_fs;
			Readonly_value_file_system<unsigned>  &_optr_fifo_samples_fs;
			Value_file_system<unsigned>           &_play_underruns_fs;

			Info(Readonly_value_file_system<unsigned>  &channels_fs,
			     Readonly_value_file_system<unsigned>  &format_fs,
			     Readonly_value_file_system<unsigned>  &sample_rate_fs,
			     Value_file_system<unsigned>           &ofrag_total_fs,
			     Value_file_system<unsigned>           &ofrag_size_fs,
			     Readonly_value_file_system<unsigned>  &ofrag_avail_fs,
			     Readonly_value_file_system<long long> &optr_samples_fs,
			     Readonly_value_file_system<unsigned>  &optr_fifo_samples_fs,
			     Value_file_system<unsigned>           &play_underruns_fs)
			:
				channels              { 0 },
				format                { 0 },
				sample_rate           { 0 },
				ofrag_total           { 0 },
				ofrag_size            { 0 },
				ofrag_avail           { 0 },
				optr_samples          { 0 },
				optr_fifo_samples     { 0 },
				play_underruns        { 0 },
				_channels_fs          { channels_fs },
				_format_fs            { format_fs },
				_sample_rate_fs       { sample_rate_fs },
				_ofrag_total_fs       { ofrag_total_fs },
				_ofrag_size_fs        { ofrag_size_fs },
				_ofrag_avail_fs       { ofrag_avail_fs },
				_optr_samples_fs      { optr_samples_fs },
				_optr_fifo_samples_fs { optr_fifo_samples_fs },
				_play_underruns_fs    { play_underruns_fs }
			{ }

			void update()
			{
				_channels_fs         .value(channels);
				_format_fs           .value(format);
				_sample_rate_fs      .value(sample_rate);
				_ofrag_total_fs      .value(ofrag_total);
				_ofrag_size_fs       .value(ofrag_size);
				_ofrag_avail_fs      .value(ofrag_avail);
				_optr_samples_fs     .value(optr_samples);
				_optr_fifo_samples_fs.value(optr_fifo_samples);
				_play_underruns_fs   .value(play_underruns);
			}

			void print(Genode::Output &out) const
			{
				char buf[256] { };

				Genode::Xml_generator xml(buf, sizeof(buf), "oss", [&] () {
					xml.attribute("channels",          channels);
					xml.attribute("format",            format);
					xml.attribute("sample_rate",       sample_rate);
					xml.attribute("ofrag_total",       ofrag_total);
					xml.attribute("ofrag_size",        ofrag_size);
					xml.attribute("ofrag_avail",       ofrag_avail);
					xml.attribute("optr_samples",      optr_samples);
					xml.attribute("optr_fifo_samples", optr_fifo_samples);
					xml.attribute("play_underruns",    play_underruns);
				});

				Genode::print(out, Genode::Cstring(buf));
			}
		};

	private:

		Audio(Audio const &);
		Audio &operator = (Audio const &);

		bool _started     { false };

		enum { CHANNELS = 2, };
		const char *_channel_names[CHANNELS] = { "front left", "front right" };

		Genode::Constructible<Audio_out::Connection> _out[CHANNELS];

		Info &_info;
		Readonly_value_file_system<Info, 256> &_info_fs;

	public:

		Audio(Genode::Env &env,
		      Info        &info,
		      Readonly_value_file_system<Info, 256> &info_fs)
		:
			_info         { info },
			_info_fs      { info_fs }
		{
			for (int i = 0; i < CHANNELS; i++) {
				try {
					_out[i].construct(env, _channel_names[i], false, false);
				} catch (...) {
					Genode::error("could not create Audio_out channel ", i);
					throw;
				}
			}

			_info.channels    = CHANNELS;
			_info.format      = (unsigned)AFMT_S16_LE;
			_info.sample_rate = Audio_out::SAMPLE_RATE;
			_info.ofrag_total = Audio_out::QUEUE_SIZE;
			_info.ofrag_size  =
				(unsigned)Audio_out::PERIOD * (unsigned)CHANNELS
				                            * sizeof (int16_t);
			_info.ofrag_avail = _info.ofrag_total;
			_info.update();
			_info_fs.value(_info);
		}

		void alloc_sigh(Genode::Signal_context_capability sigh)
		{
			_out[0]->alloc_sigh(sigh);
		}

		void progress_sigh(Genode::Signal_context_capability sigh)
		{
			_out[0]->progress_sigh(sigh);
		}

		void pause()
		{
			for (int i = 0; i < CHANNELS; i++) {
				_out[i]->stop();
			}

			_started = false;
		}

		unsigned queued() const
		{
			return _out[0]->stream()->queued();
		}

		void update_info_ofrag_avail_from_optr_fifo_samples()
		{
			unsigned const samples_per_fragment =
				_info.ofrag_size / (CHANNELS * sizeof(int16_t));
			_info.ofrag_avail = _info.ofrag_total -
				((_info.optr_fifo_samples / samples_per_fragment) +
				 ((_info.optr_fifo_samples % samples_per_fragment) ? 1 : 0));
		}

		/*
		 * Handle progress signal.
		 *
		 * Returns true if at least one stream packet is available.
		 */
		bool handle_progress()
		{
			unsigned fifo_samples_new = queued() * Audio_out::PERIOD;

			if (fifo_samples_new == _info.optr_fifo_samples) {
				/*
				 * This is usually the progress signal for the first
				 * packet after 'start()', which is invalid.
				 */
				return false;
			}

			/*
			 * The queue count can wrap from 0 to 255 if packets are not
			 * submitted fast enough.
			 */

			if ((fifo_samples_new == 0) ||
			    (fifo_samples_new > _info.optr_fifo_samples)) {
				pause();
				if (fifo_samples_new > _info.optr_fifo_samples) {
					_info.play_underruns++;
					fifo_samples_new = 0;
				}
			}

			_info.optr_fifo_samples = fifo_samples_new;
			update_info_ofrag_avail_from_optr_fifo_samples();
			_info.update();
			_info_fs.value(_info);

			return true;
		}

		bool write(char const *buf, file_size buf_size, file_size &out_size)
		{
			using namespace Genode;

			bool block_write = false;

			/*
			 * Calculate how many strean packets would be needed to
			 * write the buffer.
			 */

			unsigned stream_packets_to_write =
				(buf_size / _stream_packet_size) + 
				((buf_size % _stream_packet_size != 0) ? 1 : 0);

			/*
			 * Calculate how many stream packets are available
			 * depending on the configured fragment count and
			 * fragment size and the number of packets already
			 * in use.
			 */

			unsigned const stream_packets_total =
				(_info.ofrag_total * _info.ofrag_size) / _stream_packet_size;

			unsigned const stream_packets_used =
				_info.optr_fifo_samples / Audio_out::PERIOD;

			unsigned const stream_packets_avail =
				stream_packets_total - stream_packets_used;

			/*
			 * If not enough stream packets are available, use the
			 * available packets and report the blocking condition
			 * to the caller.
			 */

			if (stream_packets_to_write > stream_packets_avail) {
				stream_packets_to_write = stream_packets_avail;
				buf_size = stream_packets_to_write * _stream_packet_size;
				block_write = true;
			}

			if (stream_packets_to_write == 0) {
				out_size = 0;
				throw Vfs::File_io_service::Insufficient_buffer();
			}

			if (!_started) {
				_started = true;
				_out[0]->start();
				_out[1]->start();
			}

			for (unsigned packet_count = 0;
			     packet_count < stream_packets_to_write;
			     packet_count++) {

				Audio_out::Packet *lp = nullptr;

				try { lp = _out[0]->stream()->alloc(); }
				catch (...) {
					error("stream full",
					      " queued: ", _out[0]->stream()->queued(),
					      " pos: ",    _out[0]->stream()->pos(),
					      " tail: ",   _out[0]->stream()->tail()
					);
					break;
				}

				unsigned const pos    = _out[0]->stream()->packet_position(lp);
				Audio_out::Packet *rp = _out[1]->stream()->get(pos);

				float *dest[CHANNELS] = { lp->content(), rp->content() };

				for (unsigned sample_count = 0;
				     sample_count < Audio_out::PERIOD;
				     sample_count++) {

					for (unsigned c = 0; c < CHANNELS; c++) {

						unsigned const buf_index =
							(packet_count * Audio_out::PERIOD * CHANNELS) +
							(sample_count * CHANNELS) + c;

						int16_t src_sample;
						if (buf_index * sizeof(uint16_t) < buf_size) {
							src_sample = ((int16_t const*)buf)[buf_index];
						} else {
							/*
							 * Fill up the packet with zeroes if the buffer
							 * is not aligned to minimum fragment size
							 * (packet size) granularity.
							 */
							src_sample = 0;
						}

						dest[c][sample_count] = ((float)src_sample) / 32768.0f;
					}
				}

				_out[0]->submit(lp);
				_out[1]->submit(rp);
			}

			out_size = Genode::min(stream_packets_to_write *
			                       _stream_packet_size, buf_size);

			/* update info */

			unsigned const stream_samples_written =
				stream_packets_to_write * Audio_out::PERIOD;
			_info.optr_samples += stream_samples_written;
			_info.optr_fifo_samples += stream_samples_written;
			update_info_ofrag_avail_from_optr_fifo_samples();
			_info.update();
			_info_fs.value(_info);

			if (block_write) { throw Vfs::File_io_service::Insufficient_buffer(); }

			return true;
		}
};


class Vfs::Oss_file_system::Data_file_system : public Single_file_system
{
	private:

		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Genode::Entrypoint &_ep;
		Audio              &_audio;

		struct Oss_vfs_handle : public Single_vfs_handle
		{
			Audio &_audio;

			bool blocked = false;

			Oss_vfs_handle(Directory_service      &ds,
			                    File_io_service   &fs,
			                    Genode::Allocator &alloc,
			                    int                flags,
			                    Audio             &audio)
			:
				Single_vfs_handle { ds, fs, alloc, flags },
				_audio { audio }
			{ }

			Read_result read(char *buf, file_size buf_size, file_size &out_count) override
			{
				/* dummy implementation with audible noise for testing */

				for (file_size i = 0; i < buf_size; i++)
					buf[i] = i;

				out_count = buf_size;

				return READ_OK;
			}

			Write_result write(char const *buf, file_size buf_size,
			                   file_size &out_count) override
			{
				try {
					return _audio.write(buf, buf_size, out_count) ? WRITE_OK : WRITE_ERR_INVALID;
				} catch (Vfs::File_io_service::Insufficient_buffer) {
					blocked = true;
					return WRITE_OK;
				}
			}

			bool read_ready() override
			{
				return true;
			}
		};

		using Registered_handle = Genode::Registered<Oss_vfs_handle>;
		using Handle_registry   = Genode::Registry<Registered_handle>;

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _alloc_avail_sigh {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_alloc_avail };

		void _handle_alloc_avail() { }

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _progress_sigh {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_progress };

		void _handle_progress()
		{
			if (_audio.handle_progress()) {
				/* at least one stream packet is available */
				_handle_registry.for_each([this] (Registered_handle &handle) {
					if (handle.blocked) {
						handle.blocked = false;
						handle.io_progress_response();
					}
				});
			}
		}

	public:

		Data_file_system(Genode::Entrypoint &ep,
		                 Audio              &audio,
		                 Name         const &name)
		:
			Single_file_system { Node_type::CONTINUOUS_FILE, name.string(),
			                     Node_rwx::ro(), Genode::Xml_node("<data/>") },

			_ep    { ep },
			_audio { audio }
		{
			_audio.alloc_sigh(_alloc_avail_sigh);
			_audio.progress_sigh(_progress_sigh);
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
					Registered_handle(_handle_registry, *this, *this, alloc, flags,
					                  _audio);
				return OPEN_OK;
			}
			catch (Genode::Out_of_ram)  { return OPEN_ERR_OUT_OF_RAM; }
			catch (Genode::Out_of_caps) { return OPEN_ERR_OUT_OF_CAPS; }
		}

		/********************************
		 ** File I/O service interface **
		 ********************************/

		Ftruncate_result ftruncate(Vfs_handle *, file_size) override
		{
			return FTRUNCATE_OK;
		}

		bool check_unblock(Vfs_handle *, bool, bool wr, bool) override
		{
			return wr;
		}
};


struct Vfs::Oss_file_system::Local_factory : File_system_factory
{
	using Label = Genode::String<64>;
	Label const _label;
	Name  const _name;

	Vfs::Env &_env;

	Readonly_value_file_system<unsigned>  _channels_fs          { "channels", 0U };
	Readonly_value_file_system<unsigned>  _format_fs            { "format", 0U };
	Readonly_value_file_system<unsigned>  _sample_rate_fs       { "sample_rate", 0U };
	Value_file_system<unsigned>           _ofrag_total_fs       { "ofrag_total", 0U };
	Value_file_system<unsigned>           _ofrag_size_fs        { "ofrag_size", 0U} ;
	Readonly_value_file_system<unsigned>  _ofrag_avail_fs       { "ofrag_avail", 0U };
	Readonly_value_file_system<long long> _optr_samples_fs      { "optr_samples", 0LL };
	Readonly_value_file_system<unsigned>  _optr_fifo_samples_fs { "optr_fifo_samples", 0U };
	Value_file_system<unsigned>           _play_underruns_fs    { "play_underruns", 0U };

	Audio::Info _info { _channels_fs, _format_fs, _sample_rate_fs,
	                    _ofrag_total_fs, _ofrag_size_fs, _ofrag_avail_fs,
	                    _optr_samples_fs, _optr_fifo_samples_fs,
	                    _play_underruns_fs };

	Readonly_value_file_system<Audio::Info, 256> _info_fs { "info", _info };

	Audio _audio { _env.env(), _info, _info_fs };

	Genode::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_total_handler {
		_ofrag_total_fs, "/ofrag_total",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_total_changed };

	Genode::Watch_handler<Vfs::Oss_file_system::Local_factory> _ofrag_size_handler {
		_ofrag_size_fs, "/ofrag_size",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_size_changed };

	Genode::Watch_handler<Vfs::Oss_file_system::Local_factory> _play_underruns_handler {
		_play_underruns_fs, "/play_underruns",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_play_underruns_changed };

	static constexpr size_t _native_stream_size { Audio_out::QUEUE_SIZE * _stream_packet_size };
	static constexpr size_t _ofrag_total_min { 2 };
	static constexpr size_t _ofrag_size_min { _stream_packet_size };
	static constexpr size_t _ofrag_total_max { _native_stream_size / _ofrag_size_min };
	static constexpr size_t _ofrag_size_max { _native_stream_size / _ofrag_total_min };

	/********************
	 ** Watch handlers **
	 ********************/

	void _ofrag_total_changed()
	{
		unsigned ofrag_total_new = _ofrag_total_fs.value(); 

		ofrag_total_new = Genode::max(ofrag_total_new, _ofrag_total_min);
		ofrag_total_new = Genode::min(ofrag_total_new, _ofrag_total_max);

		if (ofrag_total_new * _info.ofrag_size > _native_stream_size)
			_info.ofrag_size = 1 << Genode::log2(_native_stream_size / ofrag_total_new);

		_info.ofrag_total = ofrag_total_new;
		_info.ofrag_avail = ofrag_total_new;

		_info.update();
		_info_fs.value(_info);
	}

	void _ofrag_size_changed()
	{
		unsigned ofrag_size_new = _ofrag_size_fs.value(); 

		ofrag_size_new = Genode::max(ofrag_size_new, _ofrag_size_min);
		ofrag_size_new = Genode::min(ofrag_size_new, _ofrag_size_max);

		if (ofrag_size_new * _info.ofrag_total > _native_stream_size) {
			_info.ofrag_total = _native_stream_size / ofrag_size_new;
			_info.ofrag_avail = _info.ofrag_total;
		}

		_info.ofrag_size = ofrag_size_new;

		_info.update();
		_info_fs.value(_info);
	}

	void _play_underruns_changed()
	{
		/* reset counter */

		_info.play_underruns = 0;

		_info.update();
		_info_fs.value(_info);
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
		_data_fs { _env.env().ep(), _audio, name(config) }
	{ }

	Vfs::File_system *create(Vfs::Env&, Xml_node node) override
	{
		if (node.has_type("data")) {
			return &_data_fs;
		}

		if (node.has_type("info")) {
			return &_info_fs;
		}

		if (node.has_type(Readonly_value_file_system<unsigned>::type_name())) {

			if (_channels_fs.matches(node)) {
				return &_channels_fs;
			}

			if (_sample_rate_fs.matches(node)) {
				return &_sample_rate_fs;
			}

			if (_ofrag_avail_fs.matches(node)) {
				return &_ofrag_avail_fs;
			}

			if (_format_fs.matches(node)) {
				return &_format_fs;
			}

			if (_optr_samples_fs.matches(node)) {
				return &_optr_samples_fs;
			}

			if (_optr_fifo_samples_fs.matches(node)) {
				return &_optr_fifo_samples_fs;
			}
		}

		if (node.has_type(Value_file_system<unsigned>::type_name())) {

			if (_ofrag_total_fs.matches(node)) {
				return &_ofrag_total_fs;
			}

			if (_ofrag_size_fs.matches(node)) {
				return &_ofrag_size_fs;
			}

			if (_play_underruns_fs.matches(node)) {
				return &_play_underruns_fs;
			}
		}

		return nullptr;
	}
};


class Vfs::Oss_file_system::Compound_file_system : private Local_factory,
                                                   public  Vfs::Dir_file_system
{
	private:

		using Name = Oss_file_system::Name;

		using Config = String<512>;
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

					xml.node("readonly_value", [&] {
							 xml.attribute("name", "sample_rate");
					});

					xml.node("readonly_value", [&] {
						xml.attribute("name", "format");
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

		static const char *name() { return "oss"; }

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
