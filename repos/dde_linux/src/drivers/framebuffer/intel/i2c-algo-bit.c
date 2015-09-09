/*
 * \brief  Definition of the global 'i2c_bit_algo' struct
 * \author Norman Feske
 * \date   2015-09-11
 *
 * The 'i2c_bit_algo' struct must be defined in a C file because we cannot
 * use C-style struct initializers in C++ code.
 */

#include <lx_emul.h>

int i2c_algo_bit_xfer(struct i2c_adapter *i2c_adap, struct i2c_msg msgs[], int num)
{
	lx_printf("i2c_algo_bit_xfer called - not implemented\n");
	for (;;);
	return -1;
}


u32 i2c_algo_bit_func(struct i2c_adapter *adap)
{
	lx_printf("i2c_algo_bit_func called - not implemented\n");
	return 0;
}


const struct i2c_algorithm i2c_bit_algo = {
	.master_xfer   = i2c_algo_bit_xfer,
	.functionality = i2c_algo_bit_func,
};

