; Read-only constant upload probes. Same saved-register/SSE contract as camera_stub.asm.
EXTERN evr_body_color_trampoline:QWORD
EXTERN evr_body_depth_trampoline:QWORD
EXTERN evr_observe_body_upload:PROC
.code
evr_body_color_stub PROC
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
    mov edx, 0
    cld
    call evr_observe_body_upload
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
    jmp QWORD PTR [evr_body_color_trampoline]
evr_body_color_stub ENDP

evr_body_depth_stub PROC
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
    mov edx, 1
    cld
    call evr_observe_body_upload
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
    jmp QWORD PTR [evr_body_depth_trampoline]
evr_body_depth_stub ENDP

END
