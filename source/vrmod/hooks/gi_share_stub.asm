; Entry stubs for the GI share bridge (gi_share_bridge.hpp, 2026-10-06).
; At a hooked function entry only the argument registers RCX/RDX/R8/R9 and XMM0-XMM3 carry
; caller state (RAX/R10/R11 and flags are volatile scratch in the Win64 ABI). The stub saves
; the arguments, asks evr_gi_should_skip(index, return address, rdx, r8) and then either returns to the
; caller without running the function (its result is never used at the skipped call sites) or
; tail-jumps to the MinHook trampoline with the original arguments and stack untouched.
EXTERN evr_gi_trampolines:QWORD
EXTERN evr_gi_should_skip:PROC
.code
GI_STUB MACRO procName, gateIndex
LOCAL skipLabel
procName PROC
    push rcx
    push rdx
    push r8
    push r9
    sub rsp, 68h
    movdqu xmmword ptr [rsp+20h], xmm0
    movdqu xmmword ptr [rsp+30h], xmm1
    movdqu xmmword ptr [rsp+40h], xmm2
    movdqu xmmword ptr [rsp+50h], xmm3
    mov ecx, gateIndex
    mov rdx, qword ptr [rsp+88h]
    mov r8, qword ptr [rsp+78h]   ; the hooked function's RDX (the GI object for 73020 / 77c80's stages)
    mov r9, qword ptr [rsp+70h]   ; its R8 (the parameters of 73020)
    call evr_gi_should_skip
    movdqu xmm0, xmmword ptr [rsp+20h]
    movdqu xmm1, xmmword ptr [rsp+30h]
    movdqu xmm2, xmmword ptr [rsp+40h]
    movdqu xmm3, xmmword ptr [rsp+50h]
    add rsp, 68h
    pop r9
    pop r8
    pop rdx
    pop rcx
    test al, al
    jnz skipLabel
    jmp QWORD PTR [evr_gi_trampolines + 8*gateIndex]
skipLabel:
    ret
procName ENDP
ENDM
GI_STUB evr_gi_stub0, 0
GI_STUB evr_gi_stub1, 1
GI_STUB evr_gi_stub2, 2
GI_STUB evr_gi_stub3, 3
GI_STUB evr_gi_stub4, 4
GI_STUB evr_gi_stub5, 5
GI_STUB evr_gi_stub6, 6
GI_STUB evr_gi_stub7, 7
GI_STUB evr_gi_stub8, 8
GI_STUB evr_gi_stub9, 9
GI_STUB evr_gi_stub10, 10
GI_STUB evr_gi_stub11, 11
GI_STUB evr_gi_stub12, 12
GI_STUB evr_gi_stub13, 13
GI_STUB evr_gi_stub14, 14
GI_STUB evr_gi_stub15, 15
GI_STUB evr_gi_stub16, 16
GI_STUB evr_gi_stub17, 17
GI_STUB evr_gi_stub18, 18
GI_STUB evr_gi_stub19, 19
GI_STUB evr_gi_stub20, 20
GI_STUB evr_gi_stub21, 21
GI_STUB evr_gi_stub22, 22
GI_STUB evr_gi_stub23, 23
END
