EXTERN evr_fixture_copy:PROC
.code
PUBLIC evr_head_copy_marker
evr_head_copy_fixture PROC
    sub rsp, 28h
evr_head_copy_marker LABEL BYTE
    call evr_fixture_copy
    add rsp, 28h
    ret
evr_head_copy_fixture ENDP
END
