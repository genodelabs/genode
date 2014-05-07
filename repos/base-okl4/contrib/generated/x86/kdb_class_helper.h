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

static cmd_ret_t cmd_list_clists(cmd_group_t*);
static cmd_ret_t cmd_show_clist(cmd_group_t*);
static cmd_ret_t cmd__help(cmd_group_t*);
static cmd_ret_t cmd__abort(cmd_group_t*);
static cmd_ret_t cmd__prior(cmd_group_t*);
static cmd_ret_t cmd_mode_switch(cmd_group_t*);
static cmd_ret_t cmd_toggle_cpuprefix(cmd_group_t*);
static cmd_ret_t cmd_go(cmd_group_t*);
static cmd_ret_t cmd_arch(cmd_group_t*);
static cmd_ret_t cmd_config(cmd_group_t*);
static cmd_ret_t cmd_statistics(cmd_group_t*);
static cmd_ret_t cmd_profiling(cmd_group_t*);
static cmd_ret_t cmd_kmem_stats(cmd_group_t*);
static cmd_ret_t cmd_dump_ptab(cmd_group_t*);
static cmd_ret_t cmd_wordsize(cmd_group_t*);
static cmd_ret_t cmd_memdump(cmd_group_t*);
static cmd_ret_t cmd_memdump_remote(cmd_group_t*);
static cmd_ret_t cmd_memdump_phys(cmd_group_t*);
static cmd_ret_t cmd_show_mutexes(cmd_group_t*);
static cmd_ret_t cmd_show_dep_graph(cmd_group_t*);
static cmd_ret_t cmd_profile_print(cmd_group_t*);
static cmd_ret_t cmd_profile_enable(cmd_group_t*);
static cmd_ret_t cmd_profile_disable(cmd_group_t*);
static cmd_ret_t cmd_profile_reset(cmd_group_t*);
static cmd_ret_t cmd_reboot(cmd_group_t*);
static cmd_ret_t cmd_show_ready(cmd_group_t*);
static cmd_ret_t cmd_show_units(cmd_group_t*);
static cmd_ret_t cmd_list_spaces(cmd_group_t*);
static cmd_ret_t cmd_show_space(cmd_group_t*);
static cmd_ret_t cmd_show_tcb(cmd_group_t*);
static cmd_ret_t cmd_show_tcbext(cmd_group_t*);
static cmd_ret_t cmd_tid_format(cmd_group_t*);
static cmd_ret_t cmd_tracebuffer(cmd_group_t*);
static cmd_ret_t cmd_tb_info(cmd_group_t*);
static cmd_ret_t cmd_tb_logmask(cmd_group_t*);
static cmd_ret_t cmd_tb_dump(cmd_group_t*);
static cmd_ret_t cmd_tb_reset(cmd_group_t*);
static cmd_ret_t cmd_tracepoints(cmd_group_t*);
static cmd_ret_t cmd_tp_list(cmd_group_t*);
static cmd_ret_t cmd_tp_conf(cmd_group_t*);
static cmd_ret_t cmd_tp_conf_all(cmd_group_t*);
static cmd_ret_t cmd_tp_reset(cmd_group_t*);
static cmd_ret_t cmd_tp_enable(cmd_group_t*);
static cmd_ret_t cmd_tp_disable(cmd_group_t*);
static cmd_ret_t cmd_breakpoint(cmd_group_t*);
static cmd_ret_t cmd_singlestep(cmd_group_t*);
static cmd_ret_t cmd_branchstep(cmd_group_t*);
static cmd_ret_t cmd_reset(cmd_group_t*);
static cmd_ret_t cmd_show_ctrlregs(cmd_group_t*);
static cmd_ret_t cmd_dump_msrs(cmd_group_t*);
static cmd_ret_t cmd_dump_current_frame(cmd_group_t*);
static cmd_ret_t cmd_ports(cmd_group_t*);
static cmd_ret_t cmd_idt(cmd_group_t*);
static cmd_ret_t cmd_nmi(cmd_group_t*);
static cmd_ret_t cmd_gdt(cmd_group_t*);
static cmd_ret_t cmd_cpu(cmd_group_t*);
static cmd_ret_t cmd_show_lvt(cmd_group_t*);
static cmd_ret_t cmd_dump_frame(cmd_group_t*);
static cmd_ret_t cmd_dump_ldt(cmd_group_t*);
static cmd_ret_t cmd_apic(cmd_group_t*);
static cmd_ret_t cmd_dumpvga(cmd_group_t*);
