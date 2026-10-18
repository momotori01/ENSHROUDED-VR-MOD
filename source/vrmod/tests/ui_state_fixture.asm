PUBLIC evr_ui_state_marker
.code
evr_ui_dispatch_fixture PROC
 push r14
 sub rsp,20h
 mov r14,rcx
evr_ui_state_marker LABEL BYTE
 movzx ecx,byte ptr [r14+0ch]
 sub ecx,1
 cmp ecx,4
 ja unknown_state
 imul eax,ecx,11
 add eax,11
 jmp finish
unknown_state:
 mov eax,999
finish:
 add rsp,20h
 pop r14
 ret
evr_ui_dispatch_fixture ENDP
END
