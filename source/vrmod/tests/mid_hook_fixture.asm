EXTERN evr_fixture_observe:PROC
PUBLIC evr_fixture_marker
.code
evr_mid_fixture PROC
    push rbx
    push rbp
    push rdi
    sub rsp, 0a0h
    mov rdi, 12345678abcdef01h
    lea rbp, [rsp+100h]
    movdqu xmm0, [rcx]
    movdqu xmm1, [rcx+10h]
    movdqu xmm2, [rcx+20h]
    movdqu xmm3, [rcx+30h]
    movdqu xmm4, [rcx+40h]
    movdqu [rsp+50h], xmm0
    movdqu [rsp+60h], xmm1
    movdqu [rsp+70h], xmm2
    movdqu [rsp+80h], xmm3
    movdqu [rsp+90h], xmm4
    mov rax, 123
    mov r8, 456
    cvtsi2sd xmm5, r8
    stc
evr_fixture_marker LABEL BYTE
    mov rbx, [rsp+68h]
    lea rdx, [rbp+148h]
    adc rax, r8
    cvttsd2si r9, xmm5
    add rax, r9
    add rsp, 0a0h
    pop rdi
    pop rbp
    pop rbx
    ret
evr_mid_fixture ENDP

; Match the renderer entry's three home-slot stores. No game code is copied
; beyond this calling-convention fixture's ordinary argument stores.
evr_entry_fixture PROC
    mov [rsp+20h], r9
    mov [rsp+10h], rdx
    mov [rsp+8h], rcx
    lea rax, [rcx+rdx]
    add rax, r8
    add rax, r9
    add rax, [rsp+28h]
    ret
evr_entry_fixture ENDP

; Deliberately destroys volatile state after the C++ observer to validate the stub.
evr_observe_camera PROC
    sub rsp, 28h
    call evr_fixture_observe
    add rsp, 28h
    mov rax, 999
    mov r8, 999
    pxor xmm5, xmm5
    clc
    ret
evr_observe_camera ENDP
END
