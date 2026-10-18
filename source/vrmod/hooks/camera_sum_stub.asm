; Targeted SSE/x87 x64 mid-function observer (camera_control_observer.hpp, 1.0: the camera rotation's weighted sums at
; 0xea67ac). Upper YMM/ZMM state is not a supported hook ABI; the rotation part uses legacy SSE only.
; The callback gets the pushed registers (r15 first) and the FXSAVE64 area; a change to the saved XMM14 (the yaw sum)
; is what FXRSTOR64 gives back to the game. No breakpoint instruction or debug-register manipulation.
EXTERN evr_camera_sum_trampoline:QWORD
EXTERN evr_observe_camera_sum:PROC
.code
evr_camera_sum_stub PROC
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
    mov rcx, r11
    lea rdx, [rsp+20h]
    cld
    call evr_observe_camera_sum
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
    jmp QWORD PTR [evr_camera_sum_trampoline]
evr_camera_sum_stub ENDP
END
