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
#ifndef __TCB_LAYOUT__H__
#define __TCB_LAYOUT__H__

//#define BUILD_TCB_LAYOUT 1
    #define OFS_TCB_MYSELF_GLOBAL               0x00 /*    0 */
#define OFS_TCB_UTCB_LOCATION               0x04 /*    4 */
#define OFS_TCB_UTCB                        0x08 /*    8 */
#define OFS_TCB_SPACE                       0x0c /*   12 */
#define OFS_TCB_SPACE_ID                    0x10 /*   16 */
#define OFS_TCB_BASE_SPACE                  0x14 /*   20 */
#define OFS_TCB_PAGE_DIRECTORY              0x18 /*   24 */
#define OFS_TCB_PAGER                       0x1c /*   28 */
#define OFS_TCB_THREAD_LOCK                 0x24 /*   36 */
#define OFS_TCB_THREAD_STATE                0x28 /*   40 */
#define OFS_TCB_PARTNER                     0x2c /*   44 */
#define OFS_TCB_END_POINT                   0x30 /*   48 */
#define OFS_TCB_WAITING_FOR                 0x40 /*   64 */
#define OFS_TCB_EXCEPTION_HANDLER           0x44 /*   68 */
#define OFS_TCB_RESOURCE_BITS               0x4c /*   76 */
#define OFS_TCB_CONT                        0x50 /*   80 */
#define OFS_TCB_PREEMPTION_CONTINUATION     0x54 /*   84 */
#define OFS_TCB_ARCH                        0x58 /*   88 */
#define OFS_TCB_SUSPENDED                   0x114 /*  276 */
#define OFS_TCB_POST_SYSCALL_CALLBACK       0x118 /*  280 */
#define OFS_TCB_READY_LIST                  0x11c /*  284 */
#define OFS_TCB_BLOCKED_LIST                0x124 /*  292 */
#define OFS_TCB_MUTEXES_HEAD                0x12c /*  300 */
#define OFS_TCB_PRESENT_LIST                0x130 /*  304 */
#define OFS_TCB_BASE_PRIO                   0x138 /*  312 */
#define OFS_TCB_EFFECTIVE_PRIO              0x13c /*  316 */
#define OFS_TCB_TIMESLICE_LENGTH            0x140 /*  320 */
#define OFS_TCB_CURRENT_TIMESLICE           0x144 /*  324 */
#define OFS_TCB_SCHEDULER                   0x148 /*  328 */
#define OFS_TCB_SAVED_PARTNER               0x150 /*  336 */
#define OFS_TCB_SAVED_STATE                 0x154 /*  340 */
#define OFS_TCB_RESOURCES                   0x158 /*  344 */
#define OFS_TCB_THREAD_LIST                 0x15c /*  348 */
#define OFS_TCB_DEBUG_NAME                  0x164 /*  356 */
#define OFS_TCB_SAVED_SENT_FROM             0x174 /*  372 */
#define OFS_TCB_SYS_DATA                    0x178 /*  376 */
#define OFS_TCB_TCB_IDX                     0x1b0 /*  432 */
#define OFS_TCB_MASTER_CAP                  0x1b4 /*  436 */
#define OFS_TCB_SENT_FROM                   0x1b8 /*  440 */
#define OFS_TCB_IRQ_STACK                   0x1bc /*  444 */

#endif /* __TCB_LAYOUT__H__ */
