// Read-only discovery of the game's own XInput backend, from a live frontend scope.
// No controller substitution, OS driver installation, vibration call or state write.
namespace inputGamepad {
inline std::atomic<unsigned> failures{0};
inline void capture(uintptr_t frontend,unsigned sample) noexcept {
    if(sample>8&&sample%120)return;
    try{
        const auto input=read<uintptr_t>(frontend+0x4b0);
        if(!input)throw std::runtime_error("native input manager absent");
        const auto backend=read<uintptr_t>(input+0x48);
        if(!backend||read<uintptr_t>(backend)!=input)throw std::runtime_error("XInput backend ownership differs");
        const auto module=read<HMODULE>(backend+8);
        const auto get=read<uintptr_t>(backend+0x10),set=read<uintptr_t>(backend+0x18),caps=read<uintptr_t>(backend+0x20);
        if(!module||!get||!set||!caps)throw std::runtime_error("XInput table absent");
        const auto named=reinterpret_cast<uintptr_t>(GetProcAddress(module,"XInputGetState"));
        const auto extended=reinterpret_cast<uintptr_t>(GetProcAddress(module,MAKEINTRESOURCEA(100)));
        if((get!=named&&get!=extended)||set!=reinterpret_cast<uintptr_t>(GetProcAddress(module,"XInputSetState"))||
           caps!=reinterpret_cast<uintptr_t>(GetProcAddress(module,"XInputGetCapabilities")))throw std::runtime_error("XInput exports differ from owned table");
        using State=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
        using Capabilities=DWORD(WINAPI*)(DWORD,DWORD,XINPUT_CAPABILITIES*);
        std::ostringstream out;out<<"\"sample\":"<<sample<<",\"frontend\":"<<hex(frontend)<<",\"input\":"<<hex(input)
            <<",\"backend\":"<<hex(backend)<<",\"module\":"<<hex(reinterpret_cast<uintptr_t>(module))
            <<",\"get_state\":"<<hex(get)<<",\"get_state_ordinal100\":"<<(get==extended?"true":"false")
            <<",\"set_state\":"<<hex(set)<<",\"get_capabilities\":"<<hex(caps)<<",\"ports\":[";
        for(DWORD port=0;port<4;port++){
            XINPUT_STATE state{};XINPUT_CAPABILITIES ability{};
            const auto result=reinterpret_cast<State>(get)(port,&state);
            const auto capResult=reinterpret_cast<Capabilities>(caps)(port,0,&ability);
            if(port)out<<',';
            out<<"{\"port\":"<<port<<",\"state_result\":"<<result<<",\"capabilities_result\":"<<capResult
               <<",\"packet\":"<<state.dwPacketNumber<<",\"buttons\":"<<state.Gamepad.wButtons
               <<",\"stick\":["<<state.Gamepad.sThumbLX<<','<<state.Gamepad.sThumbLY<<','<<state.Gamepad.sThumbRX<<','<<state.Gamepad.sThumbRY
               <<"],\"triggers\":["<<unsigned(state.Gamepad.bLeftTrigger)<<','<<unsigned(state.Gamepad.bRightTrigger)
               <<"],\"type\":"<<unsigned(ability.Type)<<",\"subtype\":"<<unsigned(ability.SubType)<<'}';
        }
        const bool stable=read<uintptr_t>(frontend+0x4b0)==input&&read<uintptr_t>(input+0x48)==backend&&
            read<uintptr_t>(backend)==input&&read<HMODULE>(backend+8)==module&&read<uintptr_t>(backend+0x10)==get&&
            read<uintptr_t>(backend+0x18)==set&&read<uintptr_t>(backend+0x20)==caps;
        out<<"],\"ownership_stable\":"<<(stable?"true":"false")<<",\"read_only\":true,\"rumble_not_called\":true";
        logFile->event("input_gamepad_backend_sample",out.str());
        if(!stable)throw std::runtime_error("XInput owner changed during observation");
    }catch(const std::exception& e){if(failures.fetch_add(1)<3)try{logFile->event("input_gamepad_backend_unavailable","\"sample\":"+std::to_string(sample)+",\"reason\":"+jsonString(e.what()));}catch(...){}}
}
}
