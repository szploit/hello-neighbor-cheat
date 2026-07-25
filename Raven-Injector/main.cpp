#include <Windows.h>
#include <TlHelp32.h>

#include <filesystem>
#include <iostream>
#include <string>

DWORD find_target_process() {
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    DWORD process_id = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, L"HelloNeighbor-Win64-Shipping.exe") == 0) {
                process_id = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return process_id;
}

bool is_x64_dll(const std::filesystem::path& path) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    IMAGE_DOS_HEADER dos{};
    DWORD read = 0;
    bool valid = ReadFile(file, &dos, sizeof(dos), &read, nullptr) &&
                 read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE;
    if (valid) {
        SetFilePointer(file, dos.e_lfanew, nullptr, FILE_BEGIN);
        DWORD signature = 0;
        IMAGE_FILE_HEADER header{};
        valid = ReadFile(file, &signature, sizeof(signature), &read, nullptr) && signature == IMAGE_NT_SIGNATURE && ReadFile(file, &header, sizeof(header), &read, nullptr) && header.Machine == IMAGE_FILE_MACHINE_AMD64;
    };
    CloseHandle(file);
    return valid;
}

std::filesystem::path default_dll_path() {
    std::wstring executable(MAX_PATH, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    executable.resize(length);
    return std::filesystem::path(executable).parent_path() / L"Raven.dll";
}

bool inject_library(DWORD process_id, const std::filesystem::path& dll_path) {
    const HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process_id);
    if (!process) {
        std::wcerr << L"OpenProcess failed: " << GetLastError() << L"\n";
        return false;
    }

    const std::wstring path = dll_path.wstring();
    const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
    void* remote_path = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) {
        std::wcerr << L"VirtualAllocEx failed: " << GetLastError() << L"\n";
        CloseHandle(process);
        return false;
    }

    bool success = false;
    SIZE_T written = 0;
    if (WriteProcessMemory(process, remote_path, path.c_str(), bytes, &written) &&
        written == bytes) {
        const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        const auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(kernel32, "LoadLibraryW"));
        const HANDLE thread =
            CreateRemoteThread(process, nullptr, 0, load_library, remote_path, 0, nullptr);
        if (thread) {
            if (WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0) {
                DWORD remote_module = 0;
                GetExitCodeThread(thread, &remote_module);
                success = remote_module != 0;
            }
            CloseHandle(thread);
        } else {
            std::wcerr << L"CRT failed: " << GetLastError() << L"\n";
        }
    } else {
        std::wcerr << L"WPM failed: " << GetLastError() << L"\n";
    }

    VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    CloseHandle(process);
    return success;
}

int wmain(int argc, wchar_t** argv) {
    const std::filesystem::path dll_path = std::filesystem::absolute(argc > 1 ? argv[1] : default_dll_path());

    if (!std::filesystem::is_regular_file(dll_path)) {
        std::wcerr << L"Raven's dll was not found:\n" << dll_path << L"\n";
        return 1;
    }
    if (!is_x64_dll(dll_path)) {
        std::wcerr << L"The selected file is not an x64 DLL\n";
        return 1;
    }

    const DWORD process_id = find_target_process();
    if (!process_id) {
        std::wcerr << L"Hello neighbor" << L" is not running.\n";
        return 1;
    }

    if (!process_id) return 0;

    std::wcout << L"Loading: " << dll_path << L"\n";
    if (!inject_library(process_id, dll_path)) {
        std::wcerr << L"Injection failed";
        return 1;
    }

    std::wcout << L"Raven Loaded succesfully\n";
    return 0;
}
