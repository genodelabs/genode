#include "tss.h"

using namespace Genode;

extern char kernel_stack[];

void Tss::setup()
{
	this->rsp0 = (addr_t)kernel_stack;
	this->rsp1 = (addr_t)kernel_stack;
	this->rsp2 = (addr_t)kernel_stack;
}


void Tss::load()
{
	asm volatile ("ltr %w0" : : "r" (TSS_SELECTOR));
}
