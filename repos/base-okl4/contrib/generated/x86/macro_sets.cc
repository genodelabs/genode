/*
 * Copyright (c) 2008 Open Kernel Labs, Inc. (Copyright Holder).
 * All rights reserved.
 *
 * 1. Redistribution and use of OKL4 (Software) in source and binary
 * forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 *     (a) Redistributions of source code must retain this clause 1
 *         (including paragraphs (a), (b) and (c)), clause 2 and clause 3
 *         (Licence Terms) and the above copyright notice.
 *
 *     (b) Redistributions in binary form must reproduce the above
 *         copyright notice and the Licence Terms in the documentation and/or
 *         other materials provided with the distribution.
 *
 *     (c) Redistributions in any form must be accompanied by information on
 *         how to obtain complete source code for:
 *        (i) the Software; and
 *        (ii) all accompanying software that uses (or is intended to
 *        use) the Software whether directly or indirectly.  Such source
 *        code must:
 *        (iii) either be included in the distribution or be available
 *        for no more than the cost of distribution plus a nominal fee;
 *        and
 *        (iv) be licensed by each relevant holder of copyright under
 *        either the Licence Terms (with an appropriate copyright notice)
 *        or the terms of a licence which is approved by the Open Source
 *        Initative.  For an executable file, "complete source code"
 *        means the source code for all modules it contains and includes
 *        associated build and other files reasonably required to produce
 *        the executable.
 *
 * 2. THIS SOFTWARE IS PROVIDED ``AS IS'' AND, TO THE EXTENT PERMITTED BY
 * LAW, ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED.  WHERE ANY WARRANTY IS
 * IMPLIED AND IS PREVENTED BY LAW FROM BEING DISCLAIMED THEN TO THE
 * EXTENT PERMISSIBLE BY LAW: (A) THE WARRANTY IS READ DOWN IN FAVOUR OF
 * THE COPYRIGHT HOLDER (AND, IN THE CASE OF A PARTICIPANT, THAT
 * PARTICIPANT) AND (B) ANY LIMITATIONS PERMITTED BY LAW (INCLUDING AS TO
 * THE EXTENT OF THE WARRANTY AND THE REMEDIES AVAILABLE IN THE EVENT OF
 * BREACH) ARE DEEMED PART OF THIS LICENCE IN A FORM MOST FAVOURABLE TO
 * THE COPYRIGHT HOLDER (AND, IN THE CASE OF A PARTICIPANT, THAT
 * PARTICIPANT). IN THE LICENCE TERMS, "PARTICIPANT" INCLUDES EVERY
 * PERSON WHO HAS CONTRIBUTED TO THE SOFTWARE OR WHO HAS BEEN INVOLVED IN
 * THE DISTRIBUTION OR DISSEMINATION OF THE SOFTWARE.
 *
 * 3. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ANY OTHER PARTICIPANT BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* machine-generated file - do NOT edit */ 
/* Types? Where we're going we don't need types... */ 

#include <l4.h> 

/* Begin Macro Set __kdb_group_config */ 
/* externs for macro set: __kdb_group_config */ 
extern word_t __setentry___kdb_group_config___kdb_config_cmd__abort;
extern word_t __setentry___kdb_group_config___kdb_config_cmd__help;
extern word_t __setentry___kdb_group_config___kdb_config_cmd__prior;
extern word_t __setentry___kdb_group_config___kdb_config_cmd_mode_switch;
extern word_t __setentry___kdb_group_config___kdb_config_cmd_tid_format;
extern word_t __setentry___kdb_group_config___kdb_config_cmd_wordsize;
/* set array for macro set: __kdb_group_config */ 
word_t * __macro_set___kdb_group_config_array[] = { 
	&__setentry___kdb_group_config___kdb_config_cmd__abort,
	&__setentry___kdb_group_config___kdb_config_cmd__help,
	&__setentry___kdb_group_config___kdb_config_cmd__prior,
	&__setentry___kdb_group_config___kdb_config_cmd_mode_switch,
	&__setentry___kdb_group_config___kdb_config_cmd_tid_format,
	&__setentry___kdb_group_config___kdb_config_cmd_wordsize,
 NULL }; /* end set array for __kdb_group_config */

/* set count for macro set: __kdb_group_config */ 
word_t __macro_set___kdb_group_config_count = (sizeof(__macro_set___kdb_group_config_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_config */ 


/* Begin Macro Set __kmem_groups */ 
/* externs for macro set: __kmem_groups */ 
extern word_t __setentry___kmem_groups___kmem_group_kmem_clist;
extern word_t __setentry___kmem_groups___kmem_group_kmem_clistids;
extern word_t __setentry___kmem_groups___kmem_group_kmem_ll;
extern word_t __setentry___kmem_groups___kmem_group_kmem_misc;
extern word_t __setentry___kmem_groups___kmem_group_kmem_mutex;
extern word_t __setentry___kmem_groups___kmem_group_kmem_mutexids;
extern word_t __setentry___kmem_groups___kmem_group_kmem_pgtab;
extern word_t __setentry___kmem_groups___kmem_group_kmem_resources;
extern word_t __setentry___kmem_groups___kmem_group_kmem_root_clist;
extern word_t __setentry___kmem_groups___kmem_group_kmem_space;
extern word_t __setentry___kmem_groups___kmem_group_kmem_spaceids;
extern word_t __setentry___kmem_groups___kmem_group_kmem_stack;
extern word_t __setentry___kmem_groups___kmem_group_kmem_tcb;
extern word_t __setentry___kmem_groups___kmem_group_kmem_trace;
extern word_t __setentry___kmem_groups___kmem_group_kmem_utcb;
/* set array for macro set: __kmem_groups */ 
word_t * __macro_set___kmem_groups_array[] = { 
	&__setentry___kmem_groups___kmem_group_kmem_clist,
	&__setentry___kmem_groups___kmem_group_kmem_clistids,
	&__setentry___kmem_groups___kmem_group_kmem_ll,
	&__setentry___kmem_groups___kmem_group_kmem_misc,
	&__setentry___kmem_groups___kmem_group_kmem_mutex,
	&__setentry___kmem_groups___kmem_group_kmem_mutexids,
	&__setentry___kmem_groups___kmem_group_kmem_pgtab,
	&__setentry___kmem_groups___kmem_group_kmem_resources,
	&__setentry___kmem_groups___kmem_group_kmem_root_clist,
	&__setentry___kmem_groups___kmem_group_kmem_space,
	&__setentry___kmem_groups___kmem_group_kmem_spaceids,
	&__setentry___kmem_groups___kmem_group_kmem_stack,
	&__setentry___kmem_groups___kmem_group_kmem_tcb,
	&__setentry___kmem_groups___kmem_group_kmem_trace,
	&__setentry___kmem_groups___kmem_group_kmem_utcb,
 NULL }; /* end set array for __kmem_groups */

/* set count for macro set: __kmem_groups */ 
word_t __macro_set___kmem_groups_count = (sizeof(__macro_set___kmem_groups_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kmem_groups */ 


/* Begin Macro Set __kdb_group_arch */ 
/* externs for macro set: __kdb_group_arch */ 
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd__abort;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd__help;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd__prior;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_apic;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_branchstep;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_breakpoint;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_cpu;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_dump_ldt;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_dump_msrs;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_dumpvga;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_gdt;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_idt;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_nmi;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_ports;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_show_ctrlregs;
extern word_t __setentry___kdb_group_arch___kdb_arch_cmd_singlestep;
/* set array for macro set: __kdb_group_arch */ 
word_t * __macro_set___kdb_group_arch_array[] = { 
	&__setentry___kdb_group_arch___kdb_arch_cmd__abort,
	&__setentry___kdb_group_arch___kdb_arch_cmd__help,
	&__setentry___kdb_group_arch___kdb_arch_cmd__prior,
	&__setentry___kdb_group_arch___kdb_arch_cmd_apic,
	&__setentry___kdb_group_arch___kdb_arch_cmd_branchstep,
	&__setentry___kdb_group_arch___kdb_arch_cmd_breakpoint,
	&__setentry___kdb_group_arch___kdb_arch_cmd_cpu,
	&__setentry___kdb_group_arch___kdb_arch_cmd_dump_ldt,
	&__setentry___kdb_group_arch___kdb_arch_cmd_dump_msrs,
	&__setentry___kdb_group_arch___kdb_arch_cmd_dumpvga,
	&__setentry___kdb_group_arch___kdb_arch_cmd_gdt,
	&__setentry___kdb_group_arch___kdb_arch_cmd_idt,
	&__setentry___kdb_group_arch___kdb_arch_cmd_nmi,
	&__setentry___kdb_group_arch___kdb_arch_cmd_ports,
	&__setentry___kdb_group_arch___kdb_arch_cmd_show_ctrlregs,
	&__setentry___kdb_group_arch___kdb_arch_cmd_singlestep,
 NULL }; /* end set array for __kdb_group_arch */

/* set count for macro set: __kdb_group_arch */ 
word_t __macro_set___kdb_group_arch_count = (sizeof(__macro_set___kdb_group_arch_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_arch */ 


/* Begin Macro Set tracepoint_set */ 
/* externs for macro set: tracepoint_set */ 
extern word_t __setentry_tracepoint_set___tracepoint_DEADLOCK_DETECTED;
extern word_t __setentry_tracepoint_set___tracepoint_EXCEPTION_IPC;
extern word_t __setentry_tracepoint_set___tracepoint_FPAGE_MAP;
extern word_t __setentry_tracepoint_set___tracepoint_FPAGE_OVERMAP;
extern word_t __setentry_tracepoint_set___tracepoint_FPAGE_READ;
extern word_t __setentry_tracepoint_set___tracepoint_FPAGE_UNMAP;
extern word_t __setentry_tracepoint_set___tracepoint_IA32_GP;
extern word_t __setentry_tracepoint_set___tracepoint_IA32_NOMATH;
extern word_t __setentry_tracepoint_set___tracepoint_IA32_SEGRELOAD;
extern word_t __setentry_tracepoint_set___tracepoint_IA32_UD;
extern word_t __setentry_tracepoint_set___tracepoint_INTERRUPT;
extern word_t __setentry_tracepoint_set___tracepoint_IPC_TRANSFER;
extern word_t __setentry_tracepoint_set___tracepoint_KMEM_ALLOC;
extern word_t __setentry_tracepoint_set___tracepoint_KMEM_FREE;
extern word_t __setentry_tracepoint_set___tracepoint_PAGEFAULT_KERNEL;
extern word_t __setentry_tracepoint_set___tracepoint_PAGEFAULT_USER;
extern word_t __setentry_tracepoint_set___tracepoint_PREEMPTION_FAULT;
extern word_t __setentry_tracepoint_set___tracepoint_PREEMPTION_SIGNALED;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_CACHE_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_CAP_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_EXCHANGE_REGISTERS;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_INTERRUPT_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_IPC;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_MAP_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_MEMORY_COPY;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_MUTEX;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_MUTEX_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_PLATFORM_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_SCHEDULE;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_SECURITY_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_SPACE_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_SPACE_SWITCH;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_THREAD_CONTROL;
extern word_t __setentry_tracepoint_set___tracepoint_SYSCALL_THREAD_SWITCH;
extern word_t __setentry_tracepoint_set___tracepoint_TIMESLICE_EXPIRED;
extern word_t __setentry_tracepoint_set___tracepoint_UNWIND;
/* set array for macro set: tracepoint_set */ 
word_t * __macro_set_tracepoint_set_array[] = { 
	&__setentry_tracepoint_set___tracepoint_DEADLOCK_DETECTED,
	&__setentry_tracepoint_set___tracepoint_EXCEPTION_IPC,
	&__setentry_tracepoint_set___tracepoint_FPAGE_MAP,
	&__setentry_tracepoint_set___tracepoint_FPAGE_OVERMAP,
	&__setentry_tracepoint_set___tracepoint_FPAGE_READ,
	&__setentry_tracepoint_set___tracepoint_FPAGE_UNMAP,
	&__setentry_tracepoint_set___tracepoint_IA32_GP,
	&__setentry_tracepoint_set___tracepoint_IA32_NOMATH,
	&__setentry_tracepoint_set___tracepoint_IA32_SEGRELOAD,
	&__setentry_tracepoint_set___tracepoint_IA32_UD,
	&__setentry_tracepoint_set___tracepoint_INTERRUPT,
	&__setentry_tracepoint_set___tracepoint_IPC_TRANSFER,
	&__setentry_tracepoint_set___tracepoint_KMEM_ALLOC,
	&__setentry_tracepoint_set___tracepoint_KMEM_FREE,
	&__setentry_tracepoint_set___tracepoint_PAGEFAULT_KERNEL,
	&__setentry_tracepoint_set___tracepoint_PAGEFAULT_USER,
	&__setentry_tracepoint_set___tracepoint_PREEMPTION_FAULT,
	&__setentry_tracepoint_set___tracepoint_PREEMPTION_SIGNALED,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_CACHE_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_CAP_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_EXCHANGE_REGISTERS,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_INTERRUPT_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_IPC,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_MAP_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_MEMORY_COPY,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_MUTEX,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_MUTEX_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_PLATFORM_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_SCHEDULE,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_SECURITY_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_SPACE_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_SPACE_SWITCH,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_THREAD_CONTROL,
	&__setentry_tracepoint_set___tracepoint_SYSCALL_THREAD_SWITCH,
	&__setentry_tracepoint_set___tracepoint_TIMESLICE_EXPIRED,
	&__setentry_tracepoint_set___tracepoint_UNWIND,
 NULL }; /* end set array for tracepoint_set */

/* set count for macro set: tracepoint_set */ 
word_t __macro_set_tracepoint_set_count = (sizeof(__macro_set_tracepoint_set_array) / sizeof(word_t*)) - 1;
/* End Macro Set tracepoint_set */ 


/* Begin Macro Set __kdb_group_statistics */ 
/* externs for macro set: __kdb_group_statistics */ 
extern word_t __setentry___kdb_group_statistics___kdb_statistics_cmd__abort;
extern word_t __setentry___kdb_group_statistics___kdb_statistics_cmd__help;
extern word_t __setentry___kdb_group_statistics___kdb_statistics_cmd__prior;
extern word_t __setentry___kdb_group_statistics___kdb_statistics_cmd_kmem_stats;
/* set array for macro set: __kdb_group_statistics */ 
word_t * __macro_set___kdb_group_statistics_array[] = { 
	&__setentry___kdb_group_statistics___kdb_statistics_cmd__abort,
	&__setentry___kdb_group_statistics___kdb_statistics_cmd__help,
	&__setentry___kdb_group_statistics___kdb_statistics_cmd__prior,
	&__setentry___kdb_group_statistics___kdb_statistics_cmd_kmem_stats,
 NULL }; /* end set array for __kdb_group_statistics */

/* set count for macro set: __kdb_group_statistics */ 
word_t __macro_set___kdb_group_statistics_count = (sizeof(__macro_set___kdb_group_statistics_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_statistics */ 


/* Begin Macro Set kdb_initfuncs */ 
/* externs for macro set: kdb_initfuncs */ 
/* set array for macro set: kdb_initfuncs */ 
word_t * __macro_set_kdb_initfuncs_array[] = { 
 NULL }; /* end set array for kdb_initfuncs */

/* set count for macro set: kdb_initfuncs */ 
word_t __macro_set_kdb_initfuncs_count = (sizeof(__macro_set_kdb_initfuncs_array) / sizeof(word_t*)) - 1;
/* End Macro Set kdb_initfuncs */ 


/* Begin Macro Set __kdb_group_root */ 
/* externs for macro set: __kdb_group_root */ 
extern word_t __setentry___kdb_group_root___kdb_root_cmd__abort;
extern word_t __setentry___kdb_group_root___kdb_root_cmd__help;
extern word_t __setentry___kdb_group_root___kdb_root_cmd__prior;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_arch;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_config;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_dump_current_frame;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_dump_frame;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_dump_ptab;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_go;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_list_clists;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_list_spaces;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_memdump;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_memdump_phys;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_memdump_remote;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_reboot;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_reset;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_clist;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_dep_graph;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_mutexes;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_ready;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_space;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_tcb;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_show_tcbext;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_statistics;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_tracebuffer;
extern word_t __setentry___kdb_group_root___kdb_root_cmd_tracepoints;
/* set array for macro set: __kdb_group_root */ 
word_t * __macro_set___kdb_group_root_array[] = { 
	&__setentry___kdb_group_root___kdb_root_cmd__abort,
	&__setentry___kdb_group_root___kdb_root_cmd__help,
	&__setentry___kdb_group_root___kdb_root_cmd__prior,
	&__setentry___kdb_group_root___kdb_root_cmd_arch,
	&__setentry___kdb_group_root___kdb_root_cmd_config,
	&__setentry___kdb_group_root___kdb_root_cmd_dump_current_frame,
	&__setentry___kdb_group_root___kdb_root_cmd_dump_frame,
	&__setentry___kdb_group_root___kdb_root_cmd_dump_ptab,
	&__setentry___kdb_group_root___kdb_root_cmd_go,
	&__setentry___kdb_group_root___kdb_root_cmd_list_clists,
	&__setentry___kdb_group_root___kdb_root_cmd_list_spaces,
	&__setentry___kdb_group_root___kdb_root_cmd_memdump,
	&__setentry___kdb_group_root___kdb_root_cmd_memdump_phys,
	&__setentry___kdb_group_root___kdb_root_cmd_memdump_remote,
	&__setentry___kdb_group_root___kdb_root_cmd_reboot,
	&__setentry___kdb_group_root___kdb_root_cmd_reset,
	&__setentry___kdb_group_root___kdb_root_cmd_show_clist,
	&__setentry___kdb_group_root___kdb_root_cmd_show_dep_graph,
	&__setentry___kdb_group_root___kdb_root_cmd_show_mutexes,
	&__setentry___kdb_group_root___kdb_root_cmd_show_ready,
	&__setentry___kdb_group_root___kdb_root_cmd_show_space,
	&__setentry___kdb_group_root___kdb_root_cmd_show_tcb,
	&__setentry___kdb_group_root___kdb_root_cmd_show_tcbext,
	&__setentry___kdb_group_root___kdb_root_cmd_statistics,
	&__setentry___kdb_group_root___kdb_root_cmd_tracebuffer,
	&__setentry___kdb_group_root___kdb_root_cmd_tracepoints,
 NULL }; /* end set array for __kdb_group_root */

/* set count for macro set: __kdb_group_root */ 
word_t __macro_set___kdb_group_root_count = (sizeof(__macro_set___kdb_group_root_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_root */ 


/* Begin Macro Set __kdb_group_tracepoints */ 
/* externs for macro set: __kdb_group_tracepoints */ 
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__abort;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__help;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__prior;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_conf;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_conf_all;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_disable;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_enable;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_list;
extern word_t __setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_reset;
/* set array for macro set: __kdb_group_tracepoints */ 
word_t * __macro_set___kdb_group_tracepoints_array[] = { 
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__abort,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__help,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd__prior,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_conf,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_conf_all,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_disable,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_enable,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_list,
	&__setentry___kdb_group_tracepoints___kdb_tracepoints_cmd_tp_reset,
 NULL }; /* end set array for __kdb_group_tracepoints */

/* set count for macro set: __kdb_group_tracepoints */ 
word_t __macro_set___kdb_group_tracepoints_count = (sizeof(__macro_set___kdb_group_tracepoints_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_tracepoints */ 


/* Begin Macro Set __kdb_group_tracebuf */ 
/* externs for macro set: __kdb_group_tracebuf */ 
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__abort;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__help;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__prior;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_dump;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_info;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_logmask;
extern word_t __setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_reset;
/* set array for macro set: __kdb_group_tracebuf */ 
word_t * __macro_set___kdb_group_tracebuf_array[] = { 
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__abort,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__help,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd__prior,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_dump,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_info,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_logmask,
	&__setentry___kdb_group_tracebuf___kdb_tracebuf_cmd_tb_reset,
 NULL }; /* end set array for __kdb_group_tracebuf */

/* set count for macro set: __kdb_group_tracebuf */ 
word_t __macro_set___kdb_group_tracebuf_count = (sizeof(__macro_set___kdb_group_tracebuf_array) / sizeof(word_t*)) - 1;
/* End Macro Set __kdb_group_tracebuf */ 



