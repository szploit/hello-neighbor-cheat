#include "gui.h"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "../cheat/cheat.h"

#include <atomic>

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace raven {
    PresentFn present = nullptr;
    ResizeBuffersFn resize_buffers = nullptr;

    WNDPROC wndProc = nullptr;
    HWND hwnd = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* device_context = nullptr;
    ID3D11RenderTargetView* render_target_view = nullptr;
    bool showMenu = true;
    bool mouseUnlocked = false;
    std::atomic<bool> shutting_down{ false };
    bool imgui_initialized = false;

    void toggle() noexcept
    {
        showMenu = !showMenu;
    }

    bool isvisible() noexcept
    {
        return showMenu;
    }

    LRESULT CALLBACK WndProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam) noexcept
    {
        if (showMenu && mouseUnlocked)
        {
            ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam);

            switch (message)
            {
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
                return true;
            }
        }

        return CallWindowProc(wndProc, window, message, wParam, lParam);
    }

    void init(IDXGISwapChain* swapchain) noexcept {
        DXGI_SWAP_CHAIN_DESC sd{};
        if (FAILED(swapchain->GetDesc(&sd)))
            return;
        hwnd = sd.OutputWindow;

        if (!device) {
            if (FAILED(swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&device)) || !device)
                return;
            device->GetImmediateContext(&device_context);

            ID3D11Texture2D* backBuffer = nullptr;
            if (FAILED(swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer)) || !backBuffer)
                return;
            device->CreateRenderTargetView(backBuffer, NULL, &render_target_view);
            backBuffer->Release();

            ImGui::CreateContext();
            if (!ImGui_ImplWin32_Init(hwnd))
                return;
            wndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
            if (!ImGui_ImplDX11_Init(device, device_context))
                return;
            imgui_initialized = true;
        }
    }

    HRESULT __stdcall hkPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags) noexcept {
        static bool initialized = false;
        if (shutting_down)
            return present(swapChain, SyncInterval, Flags);

        if (!initialized) {
            init(swapChain);
            initialized = imgui_initialized;
        }

        if (GetAsyncKeyState(VK_INSERT) & 1) {
            showMenu = !showMenu;
            if (!showMenu) mouseUnlocked = false;
        }

        if (GetAsyncKeyState(VK_DELETE) & 1) {
            if (showMenu) mouseUnlocked = !mouseUnlocked;
        }

        cheat::tick();
        if (!imgui_initialized || !device_context || !render_target_view)
            return present(swapChain, SyncInterval, Flags);

        if (showMenu && mouseUnlocked) {
            ReleaseCapture();
            ClipCursor(nullptr);
        }

        if (ImGui::GetCurrentContext()) {
            ImGuiIO& io = ImGui::GetIO();
            io.MouseDrawCursor = showMenu && mouseUnlocked;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (showMenu) {
            cheat::render_menu();
        }

        ImGui::Render();
        device_context->OMSetRenderTargets(1, &render_target_view, NULL);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        return present(swapChain, SyncInterval, Flags);
    }

    HRESULT __stdcall hkResize(IDXGISwapChain* swapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) noexcept {
        if (render_target_view) {
            render_target_view->Release();
            render_target_view = nullptr;
        }

        HRESULT hr = resize_buffers(swapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

        if (SUCCEEDED(hr) && device) {
            ID3D11Texture2D* pBackBuffer = nullptr;
            swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
            if (pBackBuffer) {
                device->CreateRenderTargetView(pBackBuffer, NULL, &render_target_view);
                pBackBuffer->Release();
            }
        }

        return hr;
    }

    bool setup_overlay() noexcept {
        if (MH_Initialize() != MH_OK)
            return false;

        WNDCLASSEXA wc = {
            sizeof(WNDCLASSEXA), CS_CLASSDC, DefWindowProcA, 0L, 0L,
            GetModuleHandleA(0), 0, 0, 0, 0, "Dummy", 0
        };

        RegisterClassExA(&wc);
        HWND Hwnd = CreateWindowA("Dummy", 0, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, 0, 0, wc.hInstance, 0);

        D3D_FEATURE_LEVEL featureLevel;
        const D3D_FEATURE_LEVEL flvl[] = { D3D_FEATURE_LEVEL_11_0 };

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = Hwnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* swapChain = nullptr;
        ID3D11Device* Device = nullptr;
        ID3D11DeviceContext* Context = nullptr;

        if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            flvl, 1, D3D11_SDK_VERSION, &sd, &swapChain, &Device, &featureLevel, &Context) != S_OK) {
            DestroyWindow(Hwnd);
            UnregisterClassA("Dummy", wc.hInstance);
            MH_Uninitialize();
            return false;
        }

        void** vtable = *(void***)(swapChain);
        void* present_target = vtable[8];
        void* resize_target = vtable[13];

        if (MH_CreateHook(present_target, &hkPresent, reinterpret_cast<void**>(&present)) != MH_OK)
            return false;

        if (MH_EnableHook(present_target) != MH_OK)
            return false;

        if (MH_CreateHook(resize_target, &hkResize, reinterpret_cast<void**>(&resize_buffers)) == MH_OK)
            MH_EnableHook(resize_target);

        swapChain->Release();
        Device->Release();
        Context->Release();
        DestroyWindow(Hwnd);
        UnregisterClassA("Dummy", wc.hInstance);

        return true;
    }

    void shutdown() noexcept {
        shutting_down = true;
        MH_DisableHook(MH_ALL_HOOKS);
        Sleep(100);

        if (hwnd && wndProc)
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)wndProc);

        if (imgui_initialized) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
            imgui_initialized = false;
        }
        if (render_target_view) {
            render_target_view->Release();
            render_target_view = nullptr;
        }
        if (device_context) {
            device_context->Release();
            device_context = nullptr;
        }
        if (device) {
            device->Release();
            device = nullptr;
        }
        MH_Uninitialize();
    }
}
