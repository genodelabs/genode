/*
 * \brief  testing cache-maintenance of page-table updates
 * \author Chris hofer
 */


#ifndef _SRC__SERVER__VMM__TESTER_H_
#define _SRC__SERVER__VMM__TESTER_H_

#include <base/env.h>
#include <base/attached_ram_dataspace.h>
#include <vm_session/connection.h>
#include <timer_session/connection.h>

#include <cpu.h>


#define BASE_RAM 0x40000000
#define SZ_RAM (64 * 1024 *1024)
#define SZ_SUPERPAGE 0x200000
#define TEN_SECS 10*1000*1000
#define NUM_SUPERPAGES (SZ_RAM / SZ_SUPERPAGE)
#define TEST_CASE 1

namespace Vmm {
    class Tester;
}

class Vmm::Tester {

    private:
        Genode::Env                     & _env;
        Genode::Vm_connection           & _vm;
        Cpu                             & _cpu;
        Genode::Attached_ram_dataspace  & _vm_ram;

        Timer::Connection                _timer { _env };
        Timer::One_shot_timeout<Tester>  _timeout;

        bool _rdy_for_test { true };

        bool _attach_pages { false };
        int  _num_attached_pages;
        bool _page_attached[NUM_SUPERPAGES];

        void _attach_remaining();
        void _start_test(Genode::Duration);
        void _detach_entirely();
        void _detach_individually();

        void _prepare_test_env();

    public:
        Tester (Genode::Env    & env,
                Genode::Vm_connection      & vm,
                Cpu & cpu,
                Genode::Attached_ram_dataspace & vm_ram);

        void attach_page(Genode::addr_t);



};

#endif /* _SRC__SERVER__VMM__TESTER_H_ */