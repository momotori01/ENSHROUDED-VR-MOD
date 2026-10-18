.code
PUBLIC evr_vfx_gate_marker
PUBLIC evr_vfx_draw_marker
PUBLIC evr_vfx_bindings_marker
GATE_FIXTURE MACRO procName, markerName, savedRegister
LOCAL no_draw, checked, bad, finished
procName PROC
    push savedRegister
    sub rsp, 20h
    mov savedRegister, rcx
    mov r9, 12345678h
    movq xmm2, r9
    cmp rcx, 0
markerName LABEL BYTE
    je no_draw
    mov rax, 13579h
    jmp checked
    REPT 160
        nop
    ENDM
no_draw:
    mov rax, 24680h
checked:
    movq r10, xmm2
    cmp r10, r9
    jne bad
    cmp savedRegister, rcx
    jne bad
    jmp finished
bad:
    xor eax, eax
finished:
    add rsp, 20h
    pop savedRegister
    ret
procName ENDP
ENDM
GATE_FIXTURE evr_vfx_gate_fixture, evr_vfx_gate_marker, r12
GATE_FIXTURE evr_vfx_draw_fixture, evr_vfx_draw_marker, r15
GATE_FIXTURE evr_vfx_bindings_fixture, evr_vfx_bindings_marker, r15
END
