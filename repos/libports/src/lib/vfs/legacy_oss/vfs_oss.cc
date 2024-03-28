/*
 * \brief  OSS emulation to Audio_out and Audio_in file systems
 * \author Josef Soentgen
 * \date   2018-10-25
 */

/*
 * Copyright (C) 2018-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <audio_in_session/connection.h>
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

static constexpr bool verbose_underrun { false };

static constexpr size_t _audio_in_stream_packet_size
{ Audio_in::PERIOD * Audio_in::SAMPLE_SIZE };

static constexpr size_t _audio_out_stream_packet_size
{ Audio_out::PERIOD * Audio_out::SAMPLE_SIZE };

/*
 * One packet cannot be allocated because of the ring buffer
 * implementation.
 */
static constexpr size_t _audio_in_stream_size { (Audio_in::QUEUE_SIZE - 1) *
                                                 _audio_in_stream_packet_size };

/*
 * One packet cannot be allocated because of the ring buffer
 * implementation, another packet cannot be allocated after
 * the stream is reset by 'Audio_out::Session_client::start()'.
 */
static constexpr size_t _audio_out_stream_size { (Audio_out::QUEUE_SIZE - 2) *
                                                 _audio_out_stream_packet_size };

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
			Readonly_value_file_system<unsigned>  &_sample_rate_fs;
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
			     Readonly_value_file_system<unsigned>  &sample_rate_fs,
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

		using Write_result = Vfs::File_io_service::Write_result;

	private:

		Audio(Audio const &);
		Audio &operator = (Audio const &);

		bool _audio_out_enabled { true };
		bool _audio_in_enabled  { true };

		bool _audio_out_started { false };
		bool _audio_in_started  { false };

		enum { CHANNELS = 2, };
		const char *_channel_names[CHANNELS] = { "front left", "front right" };

		Genode::Constructible<Audio_out::Connection> _out[CHANNELS];
		Genode::Constructible<Audio_in::Connection>  _in { };

		Info &_info;
		Readonly_value_file_system<Info, 512> &_info_fs;

		size_t _read_sample_offset  { 0 };
		size_t _write_sample_offset { 0 };

		void _start_input()
		{
			if (!_audio_in_started && _audio_in_enabled) {
				_in->start();
				_audio_in_started = true;
			}
		}

		void _start_output()
		{
			if (!_audio_out_started && _audio_out_enabled) {
				_out[0]->start();
				_out[1]->start();
				_audio_out_started = true;
			}
		}

	public:

		Audio(Genode::Env &env,
		      Info        &info,
		      Readonly_value_file_system<Info, 512> &info_fs)
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

			try {
				_in.construct(env, "left");
			} catch (...) {
				Genode::error("could not create Audio_in channel");
				throw;
			}

			_info.channels    = CHANNELS;
			_info.format      = (unsigned)AFMT_S16_LE;
			_info.sample_rate = Audio_out::SAMPLE_RATE;
			_info.ifrag_size  = 2048;
			_info.ifrag_total = _audio_in_stream_size / _info.ifrag_size;
			_info.ifrag_avail = 0;
			_info.ifrag_bytes = 0;
			_info.ofrag_size  = 2048;
			_info.ofrag_total = _audio_out_stream_size / _info.ofrag_size;
			_info.ofrag_avail = _info.ofrag_total;
			_info.ofrag_bytes = _info.ofrag_avail * _info.ofrag_size;
			_info.update();
			_info_fs.value(_info);
		}

		void out_progress_sigh(Genode::Signal_context_capability sigh)
		{
			_out[0]->progress_sigh(sigh);
		}

		void in_progress_sigh(Genode::Signal_context_capability sigh)
		{
			_in->progress_sigh(sigh);
		}

		void in_overrun_sigh(Genode::Signal_context_capability sigh)
		{
			_in->overrun_sigh(sigh);
		}

		bool read_ready() const
		{
			return _info.ifrag_bytes > 0;
		}

		bool write_ready() const
		{
			/* wakeup from WRITE_ERR_WOULD_BLOCK not yet supported */
			return true;
		}

		void update_info_ofrag_avail_from_optr_fifo_samples()
		{
			_info.ofrag_bytes = (_info.ofrag_total * _info.ofrag_size) -
			                    ((_info.optr_fifo_samples + _write_sample_offset) *
			                     CHANNELS * sizeof(int16_t));
			_info.ofrag_avail = _info.ofrag_bytes / _info.ofrag_size;

			_info.update();
			_info_fs.value(_info);
		}

		void halt_input()
		{
			if (_audio_in_started) {
				_in->stop();
				_in->stream()->reset();
				_read_sample_offset = 0;
				_audio_in_started = false;
				update_info_ifrag_avail();
			}
		}

		void halt_output()
		{
			if (_audio_out_started) {
				for (int i = 0; i < CHANNELS; i++)
					_out[i]->stop();
				_write_sample_offset = 0;
				_audio_out_started = false;
				_info.optr_fifo_samples = 0;
				update_info_ofrag_avail_from_optr_fifo_samples();
			}
		}

		void enable_input(bool enable)
		{
			if (enable) {
				_audio_in_enabled = true;
				_start_input();
			} else {
				halt_input();
				_audio_in_enabled = false;
			}
		}

		void enable_output(bool enable)
		{
			if (enable) {
				_audio_out_enabled = true;
				_start_output();
			} else {
				halt_output();
				_audio_out_enabled = false;
			}
		}

		/*
		 * Handle Audio_out progress signal.
		 *
		 * Returns true if at least one stream packet is available.
		 */
		bool handle_out_progress()
		{
			unsigned fifo_samples_new = _out[0]->stream()->queued() *
			                            Audio_out::PERIOD;

			if ((fifo_samples_new >= Audio_out::PERIOD) &&
			    (_write_sample_offset != 0)) {
				/* an allocated packet is part of the queued count,
				   but might not have been submitted yet */
				fifo_samples_new -= Audio_out::PERIOD;
			}

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

				halt_output();

				_write_sample_offset = 0;

				if (fifo_samples_new > _info.optr_fifo_samples) {
					_info.play_underruns++;
					fifo_samples_new = 0;
				}

				if (verbose_underrun) {
					static int play_underruns_total;
					play_underruns_total++;
					Genode::warning("vfs_oss: underrun (",
					                play_underruns_total, ")");
				}
			}

			_info.optr_fifo_samples = fifo_samples_new;
			update_info_ofrag_avail_from_optr_fifo_samples();

			return true;
		}

		void update_info_ifrag_avail()
		{
			unsigned max_queued = (_info.ifrag_total * _info.ifrag_size) /
			                      _audio_in_stream_packet_size;
			unsigned queued = _in->stream()->queued();	

			if (queued > max_queued) {

				/*
				 * Reset tail pointer to end of configured buffer
				 * to stay in bounds of the configuration.
				 */
				unsigned pos = _in->stream()->pos();
				for (unsigned int i = 0; i < max_queued; i++)
					_in->stream()->increment_position();

				_in->stream()->reset();
				_in->stream()->pos(pos);
			}

			_info.ifrag_bytes = min((_in->stream()->queued() * _audio_in_stream_packet_size) -
			                        (_read_sample_offset * Audio_in::SAMPLE_SIZE),
			                        _info.ifrag_total * _info.ifrag_size);
			_info.ifrag_avail = _info.ifrag_bytes / _info.ifrag_size;	
			_info.update();
			_info_fs.value(_info);
		}

		/*
		 * Handle Audio_in progress signal.
		 *
		 * Returns true if at least one stream packet is available.
		 */
		bool handle_in_progress()
		{
			if (_audio_in_started) {
				update_info_ifrag_avail();
				return _info.ifrag_bytes > 0;
			}

			return false;
		}

		bool read(Byte_range_ptr const &dst, size_t &out_size)
		{
			out_size = 0;

			_start_input();

			if (_info.ifrag_bytes == 0) {
				/* block */
				return true;
			}

			size_t const buf_size = min(dst.num_bytes, _info.ifrag_bytes);

			unsigned samples_to_read = buf_size / CHANNELS / sizeof(int16_t);

			if (samples_to_read == 0) {
				/* invalid argument */
				return false;
			}

			Audio_in::Stream *stream = _in->stream();

			unsigned samples_read = 0;

			/* packet loop */

			for (;;) {
				unsigned stream_pos = stream->pos();	

				Audio_in::Packet *p = stream->get(stream_pos);

				if (!p || !p->valid()) {
					update_info_ifrag_avail();
					return true;
				}

				/* sample loop */

				for (;;) {

					if (samples_read == samples_to_read) {
						update_info_ifrag_avail();
						return true;
					}

					for (unsigned c = 0; c < CHANNELS; c++) {
						unsigned const buf_index = out_size / sizeof(int16_t);
						((int16_t*)dst.start)[buf_index] = p->content()[_read_sample_offset] * 32768;
						out_size += sizeof(int16_t);
					}

					samples_read++;

					_read_sample_offset++;

					if (_read_sample_offset == Audio_in::PERIOD) {
						p->invalidate();
						p->mark_as_recorded();
						stream->increment_position();
						_read_sample_offset = 0;
						break;
					}
				}
			}
			return true;
		}

		Write_result write(Const_byte_range_ptr const &src, size_t &out_size)
		{
			using namespace Genode;

			out_size = 0;

			if (_info.ofrag_bytes == 0)
				return Write_result::WRITE_ERR_WOULD_BLOCK;

			bool block_write = false;

			size_t buf_size = src.num_bytes;

			if (buf_size > _info.ofrag_bytes) {
				buf_size = _info.ofrag_bytes;
				block_write = true;
			}

			unsigned stream_samples_to_write = buf_size / CHANNELS / sizeof(int16_t);

			if (stream_samples_to_write == 0)
				return Write_result::WRITE_ERR_INVALID;

			_start_output();

			unsigned stream_samples_written = 0;

			/* packet loop */

			for (;;) {

				Audio_out::Packet *lp = nullptr;

				if (_write_sample_offset == 0) {

					for (;;) {
						try {
							lp = _out[0]->stream()->alloc();
							break;
						}
						catch (...) {
							/* this can happen on underrun */
							_out[0]->stream()->reset();
						}
					}
				} else {
					/*
					 * Look up the previously allocated packet.
					 * The tail pointer got incremented after allocation,
					 * so we need to decrement by 1.
					 */
					unsigned const tail =
						(_out[0]->stream()->tail() +
						 Audio_out::QUEUE_SIZE - 1) %
						Audio_out::QUEUE_SIZE;
					lp = _out[0]->stream()->get(tail);
				}

				unsigned const pos    = _out[0]->stream()->packet_position(lp);
				Audio_out::Packet *rp = _out[1]->stream()->get(pos);

				float *dest[CHANNELS] = { lp->content(), rp->content() };

				/* sample loop */

				for (;;) {

					for (unsigned c = 0; c < CHANNELS; c++) {
						unsigned const buf_index = out_size / sizeof(int16_t);
						int16_t src_sample = ((int16_t const*)src.start)[buf_index];
						dest[c][_write_sample_offset] = ((float)src_sample) / 32768.0f;
						out_size += sizeof(int16_t);
					}

					stream_samples_written++;

					_write_sample_offset++;

					if (_write_sample_offset == Audio_out::PERIOD) {
						_info.optr_samples += Audio_out::PERIOD;
						_info.optr_fifo_samples += Audio_out::PERIOD;
						_out[0]->submit(lp);
						_out[1]->submit(rp);
						_write_sample_offset = 0;
						if (stream_samples_written != stream_samples_to_write)
							break;
					}

					if (stream_samples_written == stream_samples_to_write) {

						/* update info */
						update_info_ofrag_avail_from_optr_fifo_samples();

						if (block_write)
							return Write_result::WRITE_ERR_WOULD_BLOCK;

						return Write_result::WRITE_OK;
					}
				}
			}

			return Write_result::WRITE_OK;
		}
};


class Vfs::Oss_file_system::Data_file_system : public Single_file_system
{
	private:

		Data_file_system(Data_file_system const &);
		Data_file_system &operator = (Data_file_system const &);

		Genode::Entrypoint &_ep;
		Vfs::Env::User     &_vfs_user;
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

			Read_result read(Byte_range_ptr const &dst, size_t &out_count) override
			{
				if (!dst.start)
					return READ_ERR_INVALID;

				if (dst.num_bytes == 0) {
					out_count = 0;
					return READ_OK;
				}

				bool success = _audio.read(dst, out_count);

				if (success) {
					if (out_count == 0) {
						blocked = true;
						return READ_QUEUED;
					}
					return READ_OK;
				}
				return READ_ERR_INVALID;
			}

			Write_result write(Const_byte_range_ptr const &src, size_t &out_count) override
			{
				Write_result const result = _audio.write(src, out_count);

				if (result == Write_result::WRITE_ERR_WOULD_BLOCK) {
					blocked = true;
					return WRITE_OK;
				}
				return result;
			}

			bool read_ready() const override
			{
				return _audio.read_ready();
			}

			bool write_ready() const override
			{
				return _audio.write_ready();
			}
		};

		using Registered_handle = Genode::Registered<Oss_vfs_handle>;
		using Handle_registry   = Genode::Registry<Registered_handle>;

		Handle_registry _handle_registry { };

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _audio_out_progress_sigh {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_audio_out_progress };

		Genode::Io_signal_handler<Vfs::Oss_file_system::Data_file_system> _audio_in_progress_sigh {
			_ep, *this, &Vfs::Oss_file_system::Data_file_system::_handle_audio_in_progress };

		void _handle_audio_out_progress()
		{
			if (_audio.handle_out_progress())
				_vfs_user.wakeup_vfs_user();
		}

		void _handle_audio_in_progress()
		{
			if (_audio.handle_in_progress())
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
			_audio.out_progress_sigh(_audio_out_progress_sigh);
			_audio.in_progress_sigh(_audio_in_progress_sigh);
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
	Readonly_value_file_system<unsigned>  _sample_rate_fs       { "sample_rate", 0U };
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

	Audio _audio { _env.env(), _info, _info_fs };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_input_handler {
		_enable_input_fs, "/enable_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _enable_output_handler {
		_enable_output_fs, "/enable_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_enable_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_input_handler {
		_halt_input_fs, "/halt_input",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_input_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _halt_output_handler {
		_halt_output_fs, "/halt_output",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_halt_output_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ifrag_total_handler {
		_ifrag_total_fs, "/ifrag_total",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ifrag_total_changed };

	Genode::Io::Watch_handler<Vfs::Oss_file_system::Local_factory> _ifrag_size_handler {
		_ifrag_size_fs, "/ifrag_size",
		_env.alloc(),
		*this,
		&Vfs::Oss_file_system::Local_factory::_ofrag_size_changed };

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

	static constexpr size_t _ifrag_total_min { 2 };
	static constexpr size_t _ifrag_size_min { _audio_in_stream_packet_size };
	static constexpr size_t _ifrag_total_max { _audio_in_stream_size / _ifrag_size_min };
	static constexpr size_t _ifrag_size_max { _audio_in_stream_size / _ifrag_total_min };

	static constexpr size_t _ofrag_total_min { 2 };
	static constexpr size_t _ofrag_size_min { _audio_out_stream_packet_size };
	static constexpr size_t _ofrag_total_max { _audio_out_stream_size / _ofrag_size_min };
	static constexpr size_t _ofrag_size_max { _audio_out_stream_size / _ofrag_total_min };

	/********************
	 ** Watch handlers **
	 ********************/

	void _enable_input_changed()
	{
		bool enable = (bool)_enable_input_fs.value();
		_audio.enable_input(enable);
	}

	void _enable_output_changed()
	{
		bool enable = (bool)_enable_output_fs.value();
		_audio.enable_output(enable);
	}

	void _halt_input_changed()
	{
		_audio.halt_input();
	}

	void _halt_output_changed()
	{
		_audio.halt_output();
	}

	void _ifrag_total_changed()
	{
		unsigned ifrag_total_new = _ifrag_total_fs.value(); 

		ifrag_total_new = Genode::max(ifrag_total_new, _ifrag_total_min);
		ifrag_total_new = Genode::min(ifrag_total_new, _ifrag_total_max);

		if (ifrag_total_new * _info.ifrag_size > _audio_in_stream_size)
			_info.ifrag_size = 1 << Genode::log2(_audio_in_stream_size / ifrag_total_new);

		_info.ifrag_total = ifrag_total_new;
		_info.ifrag_avail = 0;
		_info.ifrag_bytes = 0;

		_info.update();
		_info_fs.value(_info);
	}

	void _ifrag_size_changed()
	{
		unsigned ifrag_size_new = _ifrag_size_fs.value(); 

		ifrag_size_new = Genode::max(ifrag_size_new, _ifrag_size_min);
		ifrag_size_new = Genode::min(ifrag_size_new, _ifrag_size_max);

		if (ifrag_size_new * _info.ifrag_total > _audio_in_stream_size) {
			_info.ifrag_total = _audio_in_stream_size / ifrag_size_new;
			_info.ifrag_avail = 0;
			_info.ifrag_bytes = 0;
		}

		_info.ifrag_size = ifrag_size_new;

		_info.update();
		_info_fs.value(_info);
	}

	void _ofrag_total_changed()
	{
		unsigned ofrag_total_new = _ofrag_total_fs.value(); 

		ofrag_total_new = Genode::max(ofrag_total_new, _ofrag_total_min);
		ofrag_total_new = Genode::min(ofrag_total_new, _ofrag_total_max);

		if (ofrag_total_new * _info.ofrag_size > _audio_out_stream_size)
			_info.ofrag_size = 1 << Genode::log2(_audio_out_stream_size / ofrag_total_new);

		_info.ofrag_total = ofrag_total_new;
		_info.ofrag_avail = ofrag_total_new;
		_info.ofrag_bytes = ofrag_total_new * _info.ofrag_size;

		_info.update();
		_info_fs.value(_info);
	}

	void _ofrag_size_changed()
	{
		unsigned ofrag_size_new = _ofrag_size_fs.value(); 

		ofrag_size_new = Genode::max(ofrag_size_new, _ofrag_size_min);
		ofrag_size_new = Genode::min(ofrag_size_new, _ofrag_size_max);

		if (ofrag_size_new * _info.ofrag_total > _audio_out_stream_size) {
			_info.ofrag_total = _audio_out_stream_size / ofrag_size_new;
			_info.ofrag_avail = _info.ofrag_total;
			_info.ofrag_bytes = _info.ofrag_total * _info.ofrag_size;
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
		return config.attribute_value("name", Name("lagacy_oss"));
	}

	Data_file_system _data_fs;

	Local_factory(Vfs::Env &env, Xml_node config)
	:
		_label   { config.attribute_value("label", Label("")) },
		_name    { name(config) },
		_env     { env },
		_data_fs { _env.env().ep(), env.user(), _audio, name(config) }
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

			if (_ifrag_avail_fs.matches(node)) {
				return &_ifrag_avail_fs;
			}

			if (_ifrag_bytes_fs.matches(node)) {
				return &_ifrag_bytes_fs;
			}

			if (_ofrag_avail_fs.matches(node)) {
				return &_ofrag_avail_fs;
			}

			if (_ofrag_bytes_fs.matches(node)) {
				return &_ofrag_bytes_fs;
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

			if (_enable_input_fs.matches(node)) {
				return &_enable_input_fs;
			}

			if (_enable_output_fs.matches(node)) {
				return &_enable_output_fs;
			}

			if (_halt_input_fs.matches(node)) {
				return &_halt_input_fs;
			}

			if (_halt_output_fs.matches(node)) {
				return &_halt_output_fs;
			}

			if (_ifrag_total_fs.matches(node)) {
				return &_ifrag_total_fs;
			}

			if (_ifrag_size_fs.matches(node)) {
				return &_ifrag_size_fs;
			}

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

					xml.node("readonly_value", [&] {
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

		static const char *name() { return "legacy_oss"; }

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
