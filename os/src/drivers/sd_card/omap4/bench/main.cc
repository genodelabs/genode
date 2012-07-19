/*
 * \brief  SD-card benchmark OMAP4 platform
 * \author Norman Feske
 * \date   2012-07-19
 */

/* Genode includes */
#include <base/sleep.h>
#include <base/printf.h>
#include <timer_session/connection.h>

/* local includes */
#include <driver.h>


class Jiffies_thread : Genode::Thread<4096>
{
	private:

		Timer::Connection    _timer;
		Genode::Lock mutable _lock;
		Genode::size_t       _milliseconds;

		enum { MS_PER_STEP = 10 };

		void entry()
		{
			for (;;) {
				_timer.msleep(MS_PER_STEP);
				{
					Genode::Lock::Guard guard(_lock);
					_milliseconds += MS_PER_STEP;
				}
			}
		}

	public:

		Jiffies_thread() : _milliseconds(0) { start(); }

		Genode::size_t milliseconds() const
		{
			Genode::Lock::Guard guard(_lock);
			return _milliseconds;
		}

		void reset()
		{
			Genode::Lock::Guard guard(_lock);
			_milliseconds = 0;
		}
};


/*
 * \param total_size    total number of bytes to read
 * \param request_size  number of bytes per request
 */
static void test_read(Block::Driver &driver,
                      Jiffies_thread &jiffies,
                      char          *out_buffer,
                      Genode::size_t total_size,
                      Genode::size_t request_size)
{
	using namespace Genode;

	PLOG("read: request_size=%zd bytes", request_size);

	jiffies.reset();

	size_t num_requests = total_size / request_size;

	for (size_t i = 0; i < num_requests; i++)
	{
		size_t const block_count  = request_size / driver.block_size();
		addr_t const block_number = i*block_count;

		driver.read(block_number, block_count, out_buffer + i*request_size);
	}

	size_t const duration_ms = jiffies.milliseconds();

	/*
	 * Convert bytes per milliseconds to kilobytes per seconds
	 *
	 * (total_size / 1024) / (duration_ms / 1000)
	 */
	size_t const total_size_kb = total_size / 1024;
	size_t const throughput_kb_per_sec = (1000*total_size_kb) / duration_ms;

	PLOG("      -> duration:   %zd ms", duration_ms);
	PLOG("         throughput: %zd KiB/sec", throughput_kb_per_sec);
}


int main(int argc, char **argv)
{
	using namespace Genode;

	printf("--- OMAP4 SD card benchmark ---\n");

	static Block::Omap4_driver driver;
	static Jiffies_thread jiffies;

	long const block_sizes[] = {
		512, 1024, 2048, 4096, 8192, 16384, 32768, 0 };

	enum { TOTAL_SIZE = 10*1024*1024 };

	static char buffer[TOTAL_SIZE];

	for (unsigned i = 0; block_sizes[i]; i++)
		test_read(driver, jiffies, buffer, TOTAL_SIZE, block_sizes[i]);

	printf("--- OMAP4 SD card benchmark finished ---\n");
	sleep_forever();
	return 0;
}
