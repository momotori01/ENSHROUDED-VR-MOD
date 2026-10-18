; Same full GPR/flags/x87/XMM save contract as camera_stub.asm. The selected
; f7c50 branch uses legacy SSE only. Callback may clear ZF in our saved flags.
EXTERN evr_vfx_gate_trampoline:QWORD
EXTERN evr_vfx_draw_trampoline:QWORD
EXTERN evr_vfx_bindings_trampoline:QWORD
EXTERN evr_vfx_model_trampolines:QWORD
EXTERN evr_vfx_extra_trampolines:QWORD
EXTERN evr_vfx_constants_trampoline:QWORD
EXTERN evr_observe_vfx_gate:PROC
.code
GATE_STUB MACRO procName, trampolineName, gateIndex
procName PROC
    pushfq
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov r11, rsp
    and rsp, -16
    sub rsp, 230h
    mov [rsp+220h], r11
    fxsave64 [rsp+20h]
    lea rcx, [r11+80h]
    mov edx, gateIndex
    cld
    call evr_observe_vfx_gate
    fxrstor64 [rsp+20h]
    mov rsp, [rsp+220h]
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp QWORD PTR [trampolineName]
procName ENDP
ENDM
GATE_STUB evr_vfx_gate_stub, evr_vfx_gate_trampoline, 0
GATE_STUB evr_vfx_draw_stub, evr_vfx_draw_trampoline, 1
GATE_STUB evr_vfx_bindings_stub, evr_vfx_bindings_trampoline, 2
; vfx_model_bridge.hpp: gate index 3 + step, trampoline slot = step
GATE_STUB evr_vfx_model_stub0, evr_vfx_model_trampolines, 3
GATE_STUB evr_vfx_model_stub1, evr_vfx_model_trampolines+8, 4
GATE_STUB evr_vfx_model_stub2, evr_vfx_model_trampolines+16, 5
GATE_STUB evr_vfx_model_stub3, evr_vfx_model_trampolines+24, 6
GATE_STUB evr_vfx_model_stub4, evr_vfx_model_trampolines+32, 7
GATE_STUB evr_vfx_model_stub5, evr_vfx_model_trampolines+40, 8
GATE_STUB evr_vfx_model_stub6, evr_vfx_model_trampolines+48, 9
GATE_STUB evr_vfx_model_stub7, evr_vfx_model_trampolines+56, 10
; vfx_gate_probe.hpp (2026-10-16): gate index 11 + site
GATE_STUB evr_vfx_extra_stub0, evr_vfx_extra_trampolines, 11
GATE_STUB evr_vfx_extra_stub1, evr_vfx_extra_trampolines+8, 12
GATE_STUB evr_vfx_extra_stub2, evr_vfx_extra_trampolines+16, 13
GATE_STUB evr_vfx_extra_stub3, evr_vfx_extra_trampolines+24, 14
; vfx_transparent_bridge.hpp (2026-10-16): the particle constants just before their copy to the GPU
GATE_STUB evr_vfx_constants_stub, evr_vfx_constants_trampoline, 15
END
