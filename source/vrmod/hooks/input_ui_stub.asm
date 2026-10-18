; The frontend input predicate of Enshrouded 1.0 (2026-10-16): the UI code computes "input is blocked by the UI" and
; stores it through a setter, set(subobject, value) at 0x9009d0, called at 0x845baa with the owning frontend object in
; rsi. The observer (hooks/input_ui_observer.hpp) hooks the setter; this stub hands the caller's rsi over as a third
; argument (rsi is callee-saved: at entry it still holds the caller's value). No registers other than r8 are touched.
EXTERN evr_input_ui_gate:PROC
.code
evr_input_ui_stub PROC
    mov r8, rsi
    jmp evr_input_ui_gate
evr_input_ui_stub ENDP
END
