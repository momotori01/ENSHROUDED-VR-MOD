PUBLIC evr_camera_call_fixture
PUBLIC evr_camera_build_fixture
PUBLIC evr_camera_fixture_return
.code
evr_camera_call_fixture PROC FRAME
    sub rsp, 0b8h
    .allocstack 0b8h
    .endprolog
    mov [rsp+50h], r8
    mov [rsp+78h], rdx
    mov [rsp+80h], rcx
    call evr_camera_build_fixture
evr_camera_fixture_return LABEL BYTE
    add rsp, 0b8h
    ret
evr_camera_call_fixture ENDP
evr_camera_build_fixture PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    mov rbx, [rdx]
    mov [rcx], rbx
    add rsp, 20h
    pop rbx
    ret
evr_camera_build_fixture ENDP
END
