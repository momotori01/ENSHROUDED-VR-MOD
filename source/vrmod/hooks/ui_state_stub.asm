; Targeted SSE/x87 x64 mid-function observer. Upper YMM/ZMM state is not a supported hook ABI.
; The selected UI dispatch contains legacy SSE instructions, not live AVX vectors.
; No breakpoint instruction or debug-register manipulation.
EXTERN evr_ui_state_trampoline:QWORD
EXTERN evr_observe_ui_state:PROC
.code
evr_ui_state_stub PROC
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
    cld
    call evr_observe_ui_state
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
    jmp QWORD PTR [evr_ui_state_trampoline]
evr_ui_state_stub ENDP
END
