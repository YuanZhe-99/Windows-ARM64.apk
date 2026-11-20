// Windows ARM64 PE code only. Fixed JNI signatures share AAPCS64 argument/result locations.
// The leaf bank branches to one common frame; both non-leaf functions carry Windows unwind data.
#include "entries.h"

__asm__(R"(
.altmacro
.text
.p2align 2
.macro jni_slot n
.p2align 4
.def winapk_jni_entry_\n; .scl 2; .type 32; .endef
.globl winapk_jni_entry_\n
winapk_jni_entry_\n:
    mov w16, #\n
    b winapk_jni_common
.endm

.macro jni_bank64 base
.irp lo,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
    jni_slot %(\base + \lo)
.endr
.endm

.irp hi,0,64,128,192,256,320,384,448,512,576,640,704,768,832,896,960,1024,1088,1152,1216,1280,1344,1408,1472,1536,1600,1664,1728,1792,1856,1920,1984
    jni_bank64 \hi
.endr

.seh_proc winapk_jni_common
winapk_jni_common:
    stp x29, x30, [sp, #-16]!
    .seh_save_fplr_x 16
    mov x29, sp
    .seh_set_fp
    sub sp, sp, #160
    .seh_stackalloc 160
    .seh_endprologue
    stp x0, x1, [sp, #0]
    stp x2, x3, [sp, #16]
    stp x4, x5, [sp, #32]
    stp x6, x7, [sp, #48]
    stp d0, d1, [sp, #64]
    stp d2, d3, [sp, #80]
    stp d4, d5, [sp, #96]
    stp d6, d7, [sp, #112]
    add x17, sp, #176
    str x17, [sp, #128]
    mov w0, w16
    mov x1, sp
    bl winapk_jni_dispatch
    ldr x0, [sp, #136]
    ldr d0, [sp, #144]
    .seh_startepilogue
    add sp, sp, #160
    .seh_stackalloc 160
    ldp x29, x30, [sp], #16
    .seh_save_fplr_x 16
    .seh_endepilogue
    ret
    .seh_endproc

.globl winapk_jni_invoke
.seh_proc winapk_jni_invoke
winapk_jni_invoke:
    stp x29, x30, [sp, #-32]!
    .seh_save_fplr_x 32
    str x19, [sp, #16]
    .seh_save_reg x19, 16
    mov x29, sp
    .seh_set_fp
    sub sp, sp, #512
    .seh_stackalloc 512
    .seh_endprologue
    mov x16, x0
    mov x19, x1
    ldr x9, [x19, #128]
    mov x10, #0
1:
    cmp x10, x2
    b.hs 2f
    ldr x11, [x9, x10, lsl #3]
    add x12, sp, x10, lsl #3
    str x11, [x12]
    add x10, x10, #1
    b 1b
2:
    ldp x0, x1, [x19, #0]
    ldp x2, x3, [x19, #16]
    ldp x4, x5, [x19, #32]
    ldp x6, x7, [x19, #48]
    ldp d0, d1, [x19, #64]
    ldp d2, d3, [x19, #80]
    ldp d4, d5, [x19, #96]
    ldp d6, d7, [x19, #112]
    blr x16
    str x0, [x19, #136]
    str d0, [x19, #144]
    .seh_startepilogue
    add sp, sp, #512
    .seh_stackalloc 512
    ldr x19, [sp, #16]
    .seh_save_reg x19, 16
    ldp x29, x30, [sp], #32
    .seh_save_fplr_x 32
    .seh_endepilogue
    ret
    .seh_endproc

.macro jni_quad n
    .quad winapk_jni_entry_\n
.endm
.macro jni_quad_bank64 base
.irp lo,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
    jni_quad %(\base + \lo)
.endr
.endm

.section .rdata,"dr"
.p2align 3
.globl winapk_jni_entries
winapk_jni_entries:
.irp hi,0,64,128,192,256,320,384,448,512,576,640,704,768,832,896,960,1024,1088,1152,1216,1280,1344,1408,1472,1536,1600,1664,1728,1792,1856,1920,1984
    jni_quad_bank64 \hi
.endr

.macro jni_symidx n
    .symidx winapk_jni_entry_\n
.endm
.macro jni_symidx_bank64 base
.irp lo,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63
    jni_symidx %(\base + \lo)
.endr
.endm

.section .gfids$y,"dr"
.irp hi,0,64,128,192,256,320,384,448,512,576,640,704,768,832,896,960,1024,1088,1152,1216,1280,1344,1408,1472,1536,1600,1664,1728,1792,1856,1920,1984
    jni_symidx_bank64 \hi
.endr
.text
)");
