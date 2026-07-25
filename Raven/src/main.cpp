#include <Windows.h>
#include "gui/gui.h"
#include "cheat/cheat.h"

int __stdcall Hook(LPVOID parameter) {
    const auto instance = static_cast<HMODULE>(parameter);
    raven::cheat::initialize();
    if (!raven::setup_overlay()) {
        raven::cheat::shutdown();
        raven::shutdown();
        FreeLibraryAndExitThread(instance, 1);
    }

    while (!(GetAsyncKeyState(VK_END) & 1))
        Sleep(50);

    raven::cheat::shutdown();
    raven::shutdown();
    FreeLibraryAndExitThread(instance, 0);
}

int __stdcall DllMain(HMODULE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        const auto thread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(Hook), instance, 0, nullptr);

        if (thread)
            CloseHandle(thread);
    }

    return TRUE;
}
