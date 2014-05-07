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

ENTRY(_start)
BOOTMEM_SIZE = 128K;
_start_text_phys = 0x00100000 + 0x200;
_start_text = _start_text_phys + 0xF0000000;
SECTIONS
{
 .text _start_text : AT (ADDR(.text) - 0xF0000000)
 {
  *(.mb_header)
  *(.text)
  *(.text.*)
  *(.gnu.linkonce.*)
  *(.spinlock)
 }
 .rodata . : AT (ADDR(.rodata) - 0xF0000000)
 {
  *(.rodata*)
 }
 .roinit : AT(ADDR(.roinit) - 0xF0000000)
 {
  *(.roinit*)
 }
 . = ALIGN(4K);
 _start_cpu_local = .;
 .cpulocal . : AT (ADDR(.cpulocal) - 0xF0000000)
 {
  *(.data.cpulocal.tcb)
  *(.data.cpulocal.utcb)
  *(.data.cpulocal)
  *(.data.ia32.cpulocal)
 }
 _end_cpu_local = .;
 . = ALIGN(4K);
 .data . : AT (ADDR(.data) - 0xF0000000)
 {
  *(.data)
         *(.data.ia32.idt);
         *(.data.ia32.exc_all);
         *(.data.ia32.exc_common);
  *(.data.*)
  _bss_start = .;
  *(.bss)
  _bss_end = .;
 }
 . = ALIGN(4K);
 .kdebug . : AT(ADDR(.kdebug) - 0xF0000000)
 {
  *(.kdebug)
  *(.kdebug-bss)
  *(.kdebug.*)
 }
 .sets . : AT(ADDR(.sets) - 0xF0000000)
 {
  . = ALIGN(16);
  _start_setlist = .;
  *(.setlist)
  _end_setlist = .;
  . = ALIGN(16);
  _start_sets = .;
  *(SORT(set_*))
  _end_sets = .;
 }
 _end_text = ALIGN(4K);
 _end_text_phys = _end_text - 0xF0000000;
 . = ALIGN(4K);
 .reserve_bootmem : AT(ADDR(.reserve_bootmem) - 0xF0000000)
 {
  *(.reserve_bootmem)
 }
 _start_init = . - 0xF0000000;
 .init (. - 0xF0000000) :
 {
  *(.init)
  *(.init.data)
  *(.init.smp)
  *(.init.*)
 }
 _end_init = .;
 /DISCARD/ :
 {
  *(*)
  *(.eh_frame)
  *(.note)
  *(.comment)
  *(.delete)
 }
 _end_text_phys = _end_text - 0xF0000000;
 _start_bootmem_phys = _start_bootmem - 0xF0000000;
 _end_bootmem_phys = _end_bootmem - 0xF0000000;
}
