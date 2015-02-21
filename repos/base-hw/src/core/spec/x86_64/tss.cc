#include "tss.h"

using namespace Genode;

extern char kernel_stack[];

__attribute__((aligned(8))) Tss Tss::_tss;

void Tss::setup()
{
	_tss.rsp0 = (addr_t)kernel_stack;
	_tss.rsp1 = (addr_t)kernel_stack;
	_tss.rsp2 = (addr_t)kernel_stack;
}


void Tss::load()
{
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}
