; Diagnostic object-write watch (object_watch.hpp, 2026-10-06).
; Entry stub: saves the argument registers, lets evr_watch_enter snapshot the watched object and replace
; the caller's return address with evr_watch_return_stub, then tail-jumps to the MinHook trampoline with
; arguments and stack untouched (works for any signature).
; Return stub: runs when the watched function returns; preserves the return registers, lets
; evr_watch_leave diff the object, and returns to the original return address it hands back.
EXTERN evr_watch_trampolines:QWORD
EXTERN evr_watch_enter:PROC
EXTERN evr_watch_leave:PROC
.code
WATCH_STUB MACRO procName, watchIndex
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
    mov ecx, watchIndex
    lea rdx, [rsp+88h]
    lea r8, [rsp+68h]
    call evr_watch_enter
    movdqu xmm0, xmmword ptr [rsp+20h]
    movdqu xmm1, xmmword ptr [rsp+30h]
    movdqu xmm2, xmmword ptr [rsp+40h]
    movdqu xmm3, xmmword ptr [rsp+50h]
    add rsp, 68h
    pop r9
    pop r8
    pop rdx
    pop rcx
    jmp QWORD PTR [evr_watch_trampolines + 8*watchIndex]
procName ENDP
ENDM
WATCH_STUB evr_watch_stub0, 0
WATCH_STUB evr_watch_stub1, 1
WATCH_STUB evr_watch_stub2, 2
WATCH_STUB evr_watch_stub3, 3
WATCH_STUB evr_watch_stub4, 4
WATCH_STUB evr_watch_stub5, 5
WATCH_STUB evr_watch_stub6, 6
WATCH_STUB evr_watch_stub7, 7
WATCH_STUB evr_watch_stub8, 8
WATCH_STUB evr_watch_stub9, 9
evr_watch_return_stub PROC
    sub rsp, 8
    push rax
    push rdx
    push rcx
    push r8
    push r9
    push r10
    push r11
    sub rsp, 60h
    movdqu xmmword ptr [rsp+20h], xmm0
    movdqu xmmword ptr [rsp+30h], xmm1
    call evr_watch_leave
    mov qword ptr [rsp+98h], rax
    movdqu xmm0, xmmword ptr [rsp+20h]
    movdqu xmm1, xmmword ptr [rsp+30h]
    add rsp, 60h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rcx
    pop rdx
    pop rax
    ret
evr_watch_return_stub ENDP
END
