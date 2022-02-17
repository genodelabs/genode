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
			if (print_error) {
				error("expected entry: ", _next_value, ", but got: ", current);
			}
			return false;
		}

		_next_value++;
		return true;
	}

	void value(Trace::Buffer::Entry const &entry) {
		_next_value = reinterpret_cast<const Entry*>(entry.data())->value; }

	void print(Output &out) const { Genode::print(out, "constant entry size"); }
};


/**
 * Variable-size entries to test wrap-around with padding
 */
struct Generator2
{
	unsigned char  _next_value  { 1 };
	size_t         _next_length { 10 };
	size_t   const _max_length  { 200 };

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
			if (print_error) {
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

	void value(Trace::Buffer::Entry const &entry)
	{
		_next_value  = reinterpret_cast<const Entry*>(entry.data())->value[0];
		_next_length = entry.length();
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
	Trace_buffer       buffer;
	unsigned           delay;
	Timer::Connection  timer     { env };
	T                  generator { };

	struct Failed : Genode::Exception { };

	Trace_buffer_monitor(Env &env, Trace::Buffer &buffer, unsigned delay)
	: env(env),
	  buffer(buffer),
	  delay(delay)
	{ }

	void test_ok()
	{
		bool done = false;

		while (!done) {
			buffer.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
				if (!entry.length() || !entry.data() || entry.length() > generator.max_len()) {
					error("Got invalid entry from for_each_new_entry()");
					throw Failed();
				}

				if (!generator.validate(entry))
					throw Failed();

				done = true;

				if (delay)
					timer.usleep(delay);

				return true;
			});
		}
	}

	void test_lost()
	{
		/* read a single entry (which has unexpected value) and stop */
		bool recalibrated = false;

		while (!recalibrated) {
			buffer.for_each_new_entry([&] (Trace::Buffer::Entry &entry) {
				if (!entry.length() || !entry.data() || entry.length() > generator.max_len()) {
					error("Got invalid entry from for_each_new_entry()");
					throw Failed();
				}

				if (generator.validate(entry, false))
					throw Failed();

				/* reset generator value */
				generator.value(entry);
				recalibrated = true;

				return false;
			});
		}
	}
};


template <typename T>
class Test_tracing
{
	private:
		size_t                   _trace_buffer_sz;
		Attached_ram_dataspace   _buffer_ds;
		Trace::Buffer           *_buffer       { _buffer_ds.local_addr<Trace::Buffer>() };
		unsigned long long      *_canary       { (unsigned long long*)(_buffer_ds.local_addr<char>()
		                                                               + _trace_buffer_sz) };
		Test_thread<T>           _thread;
		Trace_buffer_monitor<T>  _test_monitor;

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
		  _thread      (env, *_buffer, producer_delay),
		  _test_monitor(env, *_buffer, consumer_delay)
		{
			/**
			 * The canary is placed right after the trace buffer. This allows us
			 * to detect buffer overflows. By filling the canary with a bogus
			 * length value, we can also detect out-of-bounds read accesses.
			 */
			*_canary = ~0ULL;
			_buffer->init(_trace_buffer_sz);

			log("running ", _test_monitor.generator, " test");
			_thread.start();

			/* read until buffer wrapped once */
			while (_buffer->wrapped() < 1)
				_test_monitor.test_ok();

			/* make sure to continue reading after buffer wrapped */
			_test_monitor.test_ok();

			/* wait for buffer to wrap twice */
			size_t const wrapped = _buffer->wrapped();
			while (_buffer->wrapped() < wrapped + 2);

			/* read an unexpected value */
			_test_monitor.test_lost();

			/* read some more expected entries */
			_test_monitor.test_ok();

			if (*_canary != ~0ULL) {
				error("Buffer overflow, canary was overwritten with ", Hex(*_canary));
				throw Overflow();
			}

			log(_test_monitor.generator, " test succeeded\n");
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
		enum { BUFFER_SIZE = 32 * ENTRY_SIZE + sizeof(Trace::Buffer) };

		/* consume as fast as possible */
		test_1.construct(env, BUFFER_SIZE, 10000, 0);
		test_1.destruct();

		/* leave a word-sized padding at the end */
		test_1.construct(env, BUFFER_SIZE+4, 10000, 0);
		test_1.destruct();

		/* XXX also test with slower consumer than producer */

		/* variable-size test */
		test_2.construct(env, BUFFER_SIZE, 10000, 0);
		test_2.destruct();

		env.parent().exit(0);
	}
};


void Component::construct(Env &env) { static Main main(env); }
