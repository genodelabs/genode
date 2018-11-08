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
#include <gems/magic_ring_buffer.h>
#include <util/xml_generator.h>
#include <vfs/dir_file_system.h>
#include <vfs/readonly_value_file_system.h>
#include <vfs/single_file_system.h>

/* libc includes */
#include <sys/soundcard.h>


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
			using Ro_fs = Readonly_value_file_system<unsigned>;

			unsigned channels;
			unsigned format;
			unsigned sample_rate;
			unsigned queue_size;
			unsigned ofrag_size;
			unsigned ofrag_avail;

			Ro_fs &_channels_fs;
			Ro_fs &_format_fs;
			Ro_fs &_sample_rate_fs;
			Ro_fs &_queue_size_fs;
			Ro_fs &_ofrag_size_fs;
			Ro_fs &_ofrag_avail_fs;

			Info(Ro_fs &channels_fs,
			     Ro_fs &format_fs,
			     Ro_fs &queue_size_fs,
			     Ro_fs &sample_rate_fs,
			     Ro_fs &ofrag_size_fs,
			     Ro_fs &ofrag_avail_fs)
			:
				channels        { 0 },
				format          { 0 },
				sample_rate     { 0 },
				queue_size      { 0 },
				ofrag_size      { 0 },
				ofrag_avail     { 0 },
				_channels_fs    { channels_fs },
				_format_fs      { format_fs },
				_sample_rate_fs { sample_rate_fs },
				_queue_size_fs  { queue_size_fs },
				_ofrag_size_fs  { ofrag_size_fs },
				_ofrag_avail_fs { ofrag_avail_fs }
			{ }

			void update()
			{
				_channels_fs   .value(channels);
				_format_fs     .value(format);
				_sample_rate_fs.value(sample_rate);
				_queue_size_fs .value(queue_size);
				_ofrag_size_fs .value(ofrag_size);
				_ofrag_avail_fs.value(ofrag_avail);
			}

			void print(Genode::Output &out) const
			{
				char buf[256] { };

				Genode::Xml_generator xml(buf, sizeof(buf), "oss", [&] () {
					xml.attribute("channels",    channels);
					xml.attribute("format",      format);
					xml.attribute("sample_rate", sample_rate);
					xml.attribute("queue_size",  queue_size);
					xml.attribute("frag_size",   ofrag_size);
					xml.attribute("frag_avail",  ofrag_avail);
				});

				Genode::print(out, Genode::Cstring(buf));
			}
		};

	private:

		Audio(Audio const &);
		Audio &operator = (Audio const &);

		/*
		 * Staging buffer
		 *
		 * Will later be used for resampling and re-coding, for
		 * the moment it de-couples the handle from the packet-stream.
		 */
		Genode::Magic_ring_buffer<float> _left_buffer;
		Genode::Magic_ring_buffer<float> _right_buffer;

		bool _started     { false };
		bool _buffer_full { false };

		enum { CHANNELS = 2, };
		const char *_channel_names[CHANNELS] = { "front left", "front right" };

		Genode::Constructible<Audio_out::Connection> _out[CHANNELS];

		Info &_info;
		Readonly_value_file_system<Info> &_info_fs;

	public:

		Audio(Genode::Env &env,
		      Info        &info,
		      Readonly_value_file_system<Info> &info_fs)
		:
			_left_buffer  { env, 1u << 20 },
			_right_buffer { env, 1u << 20 },
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
			_info.queue_size  = Audio_out::QUEUE_SIZE;
			_info.ofrag_size  =
				(unsigned)Audio_out::PERIOD * (unsigned)CHANNELS
				                            * sizeof (short);
			_info.ofrag_avail = _info.queue_size;
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

		bool _queue_threshold_reached() const
		{
			return _out[0]->stream()->queued() > 20;
		}

		bool need_data() const
		{
			return !_queue_threshold_reached();
		}

		bool write(char const *buf, file_size buf_size, file_size &out_size)
		{
			using namespace Genode;

			bool block_write = false;

			if (_queue_threshold_reached()) {
				block_write = true;
			} else {

				out_size = 0;

				size_t const samples =
					min(_left_buffer.write_avail(), buf_size/2);

				float *dest[2] = { _left_buffer.write_addr(), _right_buffer.write_addr() };

				for (size_t i = 0; i < samples/2; i++) {

					for (int c = 0; c < CHANNELS; c++) {
						float *p = dest[c];
						int16_t const v = ((int16_t const*)buf)[i * CHANNELS + c];
						p[i] = ((float)v) / 32768.0f;
					}
				}

				_left_buffer.fill(samples/2);
				_right_buffer.fill(samples/2);

				out_size += (samples * 2);
			}

			while (_left_buffer.read_avail() >= Audio_out::PERIOD) {

				if (!_started) {
					_started = true;

					_out[0]->start();
					_out[1]->start();
				}

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

				float const *src[CHANNELS] = { _left_buffer.read_addr(),
				                               _right_buffer.read_addr() };

				lp->content(src[0], Audio_out::PERIOD);
				_left_buffer.drain(Audio_out::PERIOD);
				rp->content(src[1], Audio_out::PERIOD);
				_right_buffer.drain(Audio_out::PERIOD);

				_out[0]->submit(lp);
				_out[1]->submit(rp);
			}

			/* update */
			unsigned const new_avail = Audio_out::QUEUE_SIZE - _out[0]->stream()->queued();
			_info.ofrag_avail = new_avail;
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

			Read_result read(char *, file_size, file_size &) override
			{
				/* not supported */
				return READ_ERR_INVALID;
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
			unsigned const queued = _audio.queued();
			if (!queued) {
				_audio.pause();
			}

			_handle_registry.for_each([this] (Registered_handle &handle) {
				if (handle.blocked) {
					handle.blocked = false;
					handle.io_progress_response();
				}
			});
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

	Readonly_value_file_system<unsigned> _channels_fs    { "channels", 0U };
	Readonly_value_file_system<unsigned> _format_fs      { "format", 0U };
	Readonly_value_file_system<unsigned> _sample_rate_fs { "sample_rate", 0U };
	Readonly_value_file_system<unsigned> _queue_size_fs  { "queue_size", 0U };
	Readonly_value_file_system<unsigned> _ofrag_size_fs  { "frag_size", 0U} ;
	Readonly_value_file_system<unsigned> _ofrag_avail_fs { "frag_avail", 0U };

	Audio::Info _info { _channels_fs, _format_fs, _sample_rate_fs,
	                   _queue_size_fs, _ofrag_size_fs, _ofrag_avail_fs };

	Readonly_value_file_system<Audio::Info> _info_fs { "info", _info };

	Audio _audio { _env.env(), _info, _info_fs };

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

			if (_queue_size_fs.matches(node)) {
				return &_queue_size_fs;
			}

			if (_sample_rate_fs.matches(node)) {
				return &_sample_rate_fs;
			}

			if (_ofrag_avail_fs.matches(node)) {
				return &_ofrag_avail_fs;
			}

			if (_ofrag_size_fs.matches(node)) {
				return &_ofrag_size_fs;
			}

			if (_format_fs.matches(node)) {
				return &_format_fs;
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

					xml.node("readonly_value", [&] {
						xml.attribute("name", "queue_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "frag_size");
					});

					xml.node("readonly_value", [&] {
						 xml.attribute("name", "frag_avail");
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
