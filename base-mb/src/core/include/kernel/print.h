/*
 * \brief  Kernels syscall frontend
 * \author Martin stein
 * \date   2010.07.02
 */

#ifndef _INCLUDE__KERNEL__PRINT_H_
#define _INCLUDE__KERNEL__PRINT_H_

#include <kernel/syscalls.h>

namespace Kernel {

	class Serial_port 
	{
		/**
		 * Print constant integer < 2^4 as hexadecimal value
		 * to serial port via syscalls
		 */
		inline void _print_hex_4(unsigned char x);

		public:

			/**
			 * Print constant zero-terminated string via syscalls to serial port
			 */
			inline Serial_port &operator << (char const *s);

			/**
			 * Print constant integer < 2^32 as hexadecimal value
			 * to serial port via syscalls (no leading zeros)
			 */
			inline Serial_port &operator << (unsigned int const &x);
	};

	/**
	 * Give static 'Serial_port' reference as target for stream operators
	 */
	inline Serial_port& serial_port();
}


Kernel::Serial_port& Kernel::serial_port()
{
	static Serial_port _sp;
	return _sp;
}


void Kernel::Serial_port::_print_hex_4(unsigned char x)
{
	x &= 0x0f;
	if (x > 9)
		x += 39;

	x += 48;

	Kernel::print_char(x);
}


Kernel::Serial_port& Kernel::Serial_port::operator << (char const *s)
{ 
	while (*s) print_char(*s++);
	return *this;
}


Kernel::Serial_port& Kernel::Serial_port::operator << (unsigned int const &x)
{
	enum{
		BYTE_WIDTH = 8,
		CW         = sizeof(unsigned char)*BYTE_WIDTH,
		IW         = sizeof(unsigned int)*BYTE_WIDTH
	};

	bool leading = true;
	for (int i = IW - CW; i >= 0; i = i - CW){
		unsigned char c =(char)((x >> i) & 0xff);
		if (leading) {
			if (c == 0x00) {
				if (i == 0)
					_print_hex_4(c);
				continue;
			}
			leading = false;
			if (c < 0x10) {
				_print_hex_4(c);
				continue;
			}
		}
		_print_hex_4(c >> 4);
		_print_hex_4(c);
	}
	return *this;
}

#endif /* _INCLUDE__KERNEL__PRINT_H_  */
