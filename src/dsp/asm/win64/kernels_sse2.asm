; =============================================================================
; ClipForge — Windows x64 SSE2 Audio DSP Kernels
; Assembler: NASM (-f win64)
; Target ABI: Microsoft Windows x64 Calling Convention
;
; Register usage rules (Microsoft x64 ABI):
;   Integer/Pointer Arguments:
;     Arg 1 -> rcx
;     Arg 2 -> rdx
;     Arg 3 -> r8
;     Arg 4 -> r9
;   Floating-Point Arguments (positional slot mapping):
;     Arg 1 -> xmm0
;     Arg 2 -> xmm1
;     Arg 3 -> xmm2
;     Arg 4 -> xmm3
;   Volatile Registers (caller-saved, safe to modify in leaf functions):
;     GPR: rax, rcx, rdx, r8, r9, r10, r11
;     XMM: xmm0, xmm1, xmm2, xmm3, xmm4, xmm5
;   Nonvolatile Registers (callee-saved, NEVER touched in this file):
;     GPR: rbx, rbp, rdi, rsi, rsp, r12-r15
;     XMM: xmm6-xmm15
; =============================================================================

default rel
bits 64

section .rdata align=16
    align 16
    kAbsMaskF32:  dd 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF
    kSignMaskF32: dd 0x80000000, 0x80000000, 0x80000000, 0x80000000

section .text

global cf_peak_abs_f32
global cf_count_clipped_f32
global cf_apply_gain_clamp_f32

; =============================================================================
; extern "C" float cf_peak_abs_f32(const float* samples, uint64_t count)
;   rcx = samples (const float*, arbitrary alignment)
;   rdx = count   (uint64_t)
;   Returns: maximum |samples[i]| in xmm0
; =============================================================================
align 16
cf_peak_abs_f32:
    xorps   xmm0, xmm0              ; xmm0 = 0.0f (running max accumulator)
    test    rcx, rcx                ; check samples == nullptr
    jz      .done
    test    rdx, rdx                ; check count == 0
    jz      .done

    movaps  xmm3, [kAbsMaskF32]     ; xmm3 = 0x7FFFFFFF in all 4 lanes
    mov     r8, rdx
    shr     r8, 2                   ; r8 = count / 4 (number of 4-float SSE2 vectors)
    jz      .tail_init

.vec_loop:
    movups  xmm1, [rcx]             ; unaligned load of 4 floats
    andps   xmm1, xmm3              ; clear sign bits -> |samples[i..i+3]|
    maxps   xmm0, xmm1              ; lane-wise maximum
    add     rcx, 16
    dec     r8
    jnz     .vec_loop

    ; Horizontal reduction of 4 lanes in xmm0 -> lane 0
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0x4E        ; swap high 64 bits (lanes 2,3) with low 64 bits (lanes 0,1)
    maxps   xmm0, xmm1
    movaps  xmm1, xmm0
    shufps  xmm1, xmm1, 0xB1        ; swap adjacent 32-bit lanes (1 with 0, 3 with 2)
    maxss   xmm0, xmm1              ; xmm0[0] now holds max of all 4 vector lanes

.tail_init:
    and     rdx, 3                  ; rdx = count % 4 (scalar remainder 0..3)
    jz      .done

.tail_loop:
    movss   xmm1, [rcx]
    andps   xmm1, xmm3              ; |sample|
    maxss   xmm0, xmm1
    add     rcx, 4
    dec     rdx
    jnz     .tail_loop

.done:
    ret


; =============================================================================
; extern "C" uint64_t cf_count_clipped_f32(
;     const float* samples,
;     uint64_t count,
;     float threshold
; )
;   rcx  = samples   (const float*, arbitrary alignment)
;   rdx  = count     (uint64_t)
;   xmm2 = threshold (float, 3rd parameter -> slot 2 -> xmm2 in Win64 ABI)
;   Returns: uint64_t count of samples where |sample| >= |threshold| in rax
; =============================================================================
align 16
cf_count_clipped_f32:
    xor     rax, rax                ; rax = 0 (64-bit total clipped sample counter)
    test    rcx, rcx                ; check samples == nullptr
    jz      .done
    test    rdx, rdx                ; check count == 0
    jz      .done

    movaps  xmm4, [kAbsMaskF32]     ; xmm4 = sign bit clear mask
    shufps  xmm2, xmm2, 0x00        ; broadcast threshold across all 4 lanes of xmm2
    andps   xmm2, xmm4              ; ensure threshold is non-negative (|threshold|)

    mov     r8, rdx
    shr     r8, 2                   ; r8 = total 4-float vectors (count / 4)
    jz      .tail_init

    ; Process in chunks of up to 0x3FFFFFFF vectors so 32-bit lane counters in xmm0
    ; can never overflow uint32_t before horizontal accumulation into 64-bit rax.
.chunk_loop:
    mov     r9, r8
    mov     r10, 0x3FFFFFFF
    cmp     r9, r10
    cmova   r9, r10                 ; r9 = min(r8, 0x3FFFFFFF)
    sub     r8, r9                  ; decrement remaining vectors

    pxor    xmm0, xmm0              ; xmm0 = [0, 0, 0, 0] (uint32 lane accumulators)

.vec_loop:
    movups   xmm1, [rcx]            ; load 4 floats (unaligned-safe)
    andps    xmm1, xmm4             ; xmm1 = |samples[i..i+3]|
    cmpnltps xmm1, xmm2             ; lane = 0xFFFFFFFF (-1) if |s| >= threshold, else 0
    psubd    xmm0, xmm1             ; subtract -1 -> adds +1 to each matching uint32 lane
    add      rcx, 16
    dec      r9
    jnz      .vec_loop

    ; Horizontally sum the four uint32 lanes of xmm0 into 64-bit rax
    movdqa  xmm1, xmm0
    psrldq  xmm1, 8                 ; shift upper 2 dwords down
    paddd   xmm0, xmm1
    movdqa  xmm1, xmm0
    psrldq  xmm1, 4                 ; shift dword 1 down to dword 0
    paddd   xmm0, xmm1
    movd    r11d, xmm0              ; zero-extend 32-bit sum to 64-bit r11
    add     rax, r11

    test    r8, r8
    jnz     .chunk_loop

.tail_init:
    and     rdx, 3                  ; rdx = count % 4 (scalar remainder 0..3)
    jz      .done

.tail_loop:
    movss   xmm1, [rcx]
    andps   xmm1, xmm4              ; |sample|
    xor     r11d, r11d
    comiss  xmm1, xmm2              ; compare scalar |sample| with |threshold|
    setae   r11b                    ; r11 = 1 if |sample| >= |threshold| (CF == 0)
    add     rax, r11
    add     rcx, 4
    dec     rdx
    jnz     .tail_loop

.done:
    ret


; =============================================================================
; extern "C" void cf_apply_gain_clamp_f32(
;     float* samples,
;     uint64_t count,
;     float gain,
;     float ceiling
; )
;   rcx  = samples (float*, arbitrary alignment)
;   rdx  = count   (uint64_t)
;   xmm2 = gain    (float, 3rd parameter -> slot 2 -> xmm2 in Win64 ABI)
;   xmm3 = ceiling (float, 4th parameter -> slot 3 -> xmm3 in Win64 ABI)
; =============================================================================
align 16
cf_apply_gain_clamp_f32:
    test    rcx, rcx                ; check samples == nullptr
    jz      .done
    test    rdx, rdx                ; check count == 0
    jz      .done

    xorps   xmm0, xmm0
    comiss  xmm3, xmm0              ; check ceiling < 0.0f
    jb      .done

    shufps  xmm2, xmm2, 0x00        ; broadcast gain across all 4 lanes of xmm2
    shufps  xmm3, xmm3, 0x00        ; broadcast +ceiling across all 4 lanes of xmm3
    movaps  xmm4, xmm3
    xorps   xmm4, [kSignMaskF32]    ; xmm4 = -ceiling in all 4 lanes

    mov     r8, rdx
    shr     r8, 2                   ; r8 = count / 4
    jz      .tail_init

.vec_loop:
    movups  xmm0, [rcx]             ; load 4 floats
    mulps   xmm0, xmm2              ; sample * gain
    maxps   xmm0, xmm4              ; clamp lower bound: max(v, -ceiling)
    minps   xmm0, xmm3              ; clamp upper bound: min(v, +ceiling)
    movups  [rcx], xmm0             ; store 4 floats
    add     rcx, 16
    dec     r8
    jnz     .vec_loop

.tail_init:
    and     rdx, 3                  ; rdx = count % 4 (scalar remainder 0..3)
    jz      .done

.tail_loop:
    movss   xmm0, [rcx]
    mulss   xmm0, xmm2
    maxss   xmm0, xmm4
    minss   xmm0, xmm3
    movss   [rcx], xmm0
    add     rcx, 4
    dec     rdx
    jnz     .tail_loop

.done:
    ret
