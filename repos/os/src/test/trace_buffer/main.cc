/*
 * \brief  Low-level trace buffer test
 * \author Johannes Schlatow
 * \date   2022-02-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/trace/buffer.h>
#include <trace/trace_buffer.h>
#include <base/attached_ram_dataspace.h>
#include <base/component.h>
#include <timer_session/connection.h>

using namespace Genode;


/**
 * Constant, word-sized entries
 */
struct Generator1
{
	size_t _next_value { 1 };

	struct Entry {
		size_t value;

		Entry(size_t v) : value(v)  { }

		void print(Output &out) const { Genode::print(out, value); }
	};

	size_t max_len() { return sizeof(Entry); }

	size_t generate(char *dst)
	{
		construct_at<Entry>(dst, _next_value);

		_next_value++;

		return sizeof(Entry);
	}

	bool validate(Trace::Buffer::Entry const &entry, bool print_error=true)
	{
		Entry const &current { *reinterpret_cast<const Entry*>(entry.data()) };
		if (current.value != _next_value) {
			if (print_error || current.value < _next_value)
				error("expected entry: ", _next_value, ", but got: ", current);

			return false;
		}

		_next_value++;
		return true;
	}

	void print(Output &out) const { Genode::print(out, "constant entry size"); }

	void skip_lost(unsigned long long count)
	{
		for (; count; count--)
			_next_value++;
	}
};


/**
 * Variable-size entries to test wrap-around with padding
 */
struct Generator2
{
	unsigned char  _next_value  { 1 };
	size_t         _next_length { 10 };
	size_t   const _max_length  { 60 };

	struct Entry {
		unsigned char value[0] { };

		Entry(unsigned char v, size_t len) { memset(value, v, len); }

		void print(Output &out) const { Genode::print(out, value[0]); }
	};

	size_t max_len() { return _max_length; }

	void _next()
	{
		_next_value++;
		_next_length = (_next_length + 10) % (_max_length+1);
		if (_next_length == 0)
			_next_length = 10;
	}

	size_t generate(char *dst)
	{
		const size_t len = _next_length;
		construct_at<Entry>(dst, _next_value, len);

		_next();
		return len;
	}

	bool validate(Trace::Buffer::Entry const &entry, bool print_error=true)
	{
		Entry const &current { *reinterpret_cast<const Entry*>(entry.data()) };
		if (current.value[0] != _next_value) {
			if (print_error || current.value[0] < _next_value) {
				error("expected entry: ", _next_value, ", but got: ", current);
			}
			return false;
		}

		if (entry.length() != _next_length) {
			if (print_error) {
				error("expected entry length: ", _next_length, ", but got: ", entry.length());
			}
			return false;
		}

		unsigned char last = current.value[entry.length()-1];
		if (last != _next_value) {
			if (print_error) {
				error("corrupted entry, expected: ", _next_value, ", but got: ", last);
			}
			return false;
		}

		_next();
		return true;
	}

	void skip_lost(unsigned long long count)
	{
		for (; count; count--)
			_next();
	}

	void print(Output &out) const { Genode::print(out, "variable entry size"); }
};


template <typename T>
struct Test_thread : Thread
{
	Env               &env;
	Trace::Buffer     &buffer;
	unsigned           delay;
	Timer::Connection  timer     { env };
	T                  generator { };
	bool               stop      { false };

	void entry() override
	{
		while (!stop) {
			char *dst = buffer.reserve(generator.max_len());
			buffer.commit(generator.generate(dst));

			if (delay)
				timer.usleep(delay);
		}
	}

	Test_thread(Env &env, Trace::Buffer &buffer, unsigned delay)
	: Thread(env, "producer", 1024 * sizeof(addr_t)),
	  env(env),
	  buffer(buffer),
	  delay(delay)
	{ }

	~Test_thread()
	{
		stop = true;
		this->join();
	}
};


template <typename T>
struct Trace_buffer_monitor
{
	Env               &env;
	Trace::Buffer     &raw_buffer;
	Trace_buffer       buffer;
	unsigned           delay;
	unsigned long long lost_count     { 0 };
	unsigned long long received_count { 0 };
	Timer::Connection  timer          { env };
	T                  generator      { };

	struct Failed : Genode::Exception { };

	Trace_buffer_monitor(Env &env, Trace::Buffer &buffer, unsigned delay)
	: env(env),
	  raw_buffer(buffer),
	  buffer(raw_buffer),
	  delay(delay)
	{ }

	unsigned long long lost_entries()
	{
		if (lost_count != raw_buffer.lost_entries()) {
			unsigned long long current_lost = raw_buffer.lost_entries() - lost_count;
			lost_count += current_lost;
			return current_lost;
		}

		return 0;
	}

	unsigned long long consumed() { return received_count; }

	bool _try_read(bool const lossy, unsigned long long &lost)
	{
		bool consumed = false;
		buffer.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
			if (!entry.length() || !entry.data() || entry.length() > generator.max_len()) {
				error("Got invalid entry from for_each_new_entry()");
				throw Failed();
			}

			consumed = true;

			if (!generator.validate(entry, !lossy)) {
				if (!lossy) throw Failed();

				lost = lost_entries();
				if (!lost) {
					error("Lost entries unexpectedly.");
					throw Failed();
				}

				generator.skip_lost(lost);

				/* abort for_each, otherwise we'd never catch up with a faster producer */
				return false;
			}

			received_count++;

			if (delay)
				timer.usleep(delay);

			return true;
		});

		return consumed;
	}

	void consume(bool lossy)
	{
		unsigned long long lost { 0 };

		while (!_try_read(lossy, lost));
	}

	void recalibrate()
	{
		unsigned long long lost { 0 };

		while (!_try_read(true, lost) || !lost);
	}
};


template <typename T>
class Test_tracing
{
	private:
		using Monitor = Constructible<Trace_buffer_monitor<T>>;

		size_t                   _trace_buffer_sz;
		Attached_ram_dataspace   _buffer_ds;
		Trace::Buffer           *_buffer       { _buffer_ds.local_addr<Trace::Buffer>() };
		unsigned long long      *_canary       { (unsigned long long*)(_buffer_ds.local_addr<char>()
		                                                               + _trace_buffer_sz) };
		Test_thread<T>           _thread;
		Monitor                  _test_monitor { };

		/*
		 * Noncopyable
		 */
		Test_tracing(Test_tracing const &);
		Test_tracing &operator = (Test_tracing const &);

	public:

		struct Overflow   : Genode::Exception { };
		struct Starvation : Genode::Exception { };

		Test_tracing(Env &env, size_t buffer_sz, unsigned producer_delay, unsigned consumer_delay)
		: _trace_buffer_sz   (buffer_sz),
		  _buffer_ds   (env.ram(), env.rm(), _trace_buffer_sz + sizeof(_canary)),
		  _thread      (env, *_buffer, producer_delay)
		{
			/* determine whether lost entries are interpreted as error */
			bool const lossy = consumer_delay >= producer_delay;

			/**
			 * The canary is placed right after the trace buffer. This allows us
			 * to detect buffer overflows. By filling the canary with a bogus
			 * length value, we can also detect out-of-bounds read accesses.
			 */
			*_canary = ~0ULL;
			_buffer->init(_trace_buffer_sz);

			_thread.start();
			_test_monitor.construct(env, *_buffer, consumer_delay);
			log("running ", _test_monitor->generator, " test");

			/* read until buffer wrapped a few times and we read 100 entries */
			while (_buffer->wrapped() < 2 ||
			       _test_monitor->consumed() < 50) {
				_test_monitor->consume(lossy);
			}

			/* sanity check if test configuration triggers overwriting during read */
			if (lossy && !_buffer->lost_entries())
				warning("Haven't lost any buffer entry during lossy test.");

			/* intentionally induce overwriting  */
			if (!lossy) {
				/* wait for buffer overwriting unconsumed entries */
				while (!_buffer->lost_entries());

				/* read expecting lost entries*/
				_test_monitor->recalibrate();

				/* read some more expected entries */
				_test_monitor->consume(false);
			}

			if (*_canary != ~0ULL) {
				error("Buffer overflow, canary was overwritten with ", Hex(*_canary));
				throw Overflow();
			}

			log(_test_monitor->generator, " test succeeded (",
			      "read: ", _test_monitor->consumed(),
			    ", lost: ", _buffer->lost_entries(), ")\n");
		}
};


struct Main
{
	Constructible<Test_tracing<Generator1>> test_1 { };
	Constructible<Test_tracing<Generator2>> test_2 { };

	Main(Env &env)
	{
		/* determine buffer size so that Generator1 entries fit perfectly */
		enum { ENTRY_SIZE = sizeof(Trace::Buffer::Entry) + sizeof(Generator1::Entry) };
		enum { BUFFER_SIZE = 32 * ENTRY_SIZE + 2*sizeof(Trace::Buffer) };

		/* consume as fast as possible */
		test_1.construct(env, BUFFER_SIZE, 10000, 0);
		test_1.destruct();

		/* leave a word-sized padding at the end and make consumer slower than producer */
		test_1.construct(env, BUFFER_SIZE+4, 5000, 10000);
		test_1.destruct();

		/* variable-size test with fast consumer*/
		test_2.construct(env, BUFFER_SIZE, 10000, 0);
		test_2.destruct();

		env.parent().exit(0);
	}
};


void Component::construct(Env &env) { static Main main(env); }
