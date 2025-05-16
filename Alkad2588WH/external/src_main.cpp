#define _HAS_PCH 0
#include <windows.h>
#include "renderer.h"
#include "hack_config.h"

typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t oPresent = nullptr;
std::unique_ptr<D3D11Renderer> g_Renderer = nullptr;

void DrawESP(BasePlayer* localPlayer, Camera* camera, GameObjectManager* gom) {
    if (!HackConfig::GetInstance().g_bESP || !localPlayer || !camera || !gom) return;
    for (int i = 0; i < gom->GetPlayerCount(); ++i) {
        BasePlayer* player = gom->GetPlayerByIndex(i);
        if (!player || player->IsLocalPlayer() || player->GetHealth() <= 0) continue;

        Vector3 pos = player->GetPosition();
        Vector3 headPos = player->GetHeadPosition();
        Vector3 screenPos = camera->WorldToScreen(pos);
        Vector3 screenHead = camera->WorldToScreen(headPos);

        if (screenPos.z > 0 && screenHead.z > 0) {
            float height = abs(screenHead.y - screenPos.y) * 1.2f;
            float width = height * 0.6f;
            g_Renderer->DrawRect(screenPos.x - width / 2, screenHead.y, screenPos.x + width / 2, screenPos.y, 
                HackConfig::GetInstance().g_fESPBox[0], HackConfig::GetInstance().g_fESPBox[1], HackConfig::GetInstance().g_fESPBox[2]);
        }
    }
    D3D11Renderer::Log("ESP работает, братишка!");
}

void Aimbot(BasePlayer* localPlayer, BasePlayer* target, Camera* camera) {
    if (!HackConfig::GetInstance().g_bAimbot || !localPlayer || !target || !camera) return;
    Vector3 targetHead = target->GetHeadPosition() + Vector3{0, 0.2f, 0};
    Vector3 screenPos = camera->WorldToScreen(targetHead);
    if (screenPos.z > 0) {
        Vector2 targetScreen = {screenPos.x, screenPos.y};
        float distance = sqrt(pow(targetScreen.x - GetSystemMetrics(SM_CXSCREEN) / 2, 2) + pow(targetScreen.y - GetSystemMetrics(SM_CYSCREEN) / 2, 2));
        if (distance < HackConfig::GetInstance().g_fAimbotDistance) {
            float dx = (targetScreen.x - GetSystemMetrics(SM_CXSCREEN) / 2) * HackConfig::GetInstance().g_fAimbotSensitivity * 0.1f;
            float dy = (targetScreen.y - GetSystemMetrics(SM_CYSCREEN) / 2) * HackConfig::GetInstance().g_fAimbotSensitivity * 0.1f;
            mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(dx), static_cast<DWORD>(dy), 0, 0);
        }
    }
    D3D11Renderer::Log("Аимбот наводит шороху!");
}

void Triggerbot(BasePlayer* localPlayer, Camera* camera, GameObjectManager* gom) {
    if (!HackConfig::GetInstance().g_bTriggerbot || !localPlayer || !camera || !gom) return;
    for (int i = 0; i < gom->GetPlayerCount(); ++i) {
        BasePlayer* target = gom->GetPlayerByIndex(i);
        if (!target || target->IsLocalPlayer() || target->GetHealth() <= 0) continue;
        Vector3 targetPos = target->GetHeadPosition();
        Vector3 screenPos = camera->WorldToScreen(targetPos);
        if (screenPos.z > 0 && abs(screenPos.x - GetSystemMetrics(SM_CXSCREEN) / 2) < 40 && abs(screenPos.y - GetSystemMetrics(SM_CYSCREEN) / 2) < 40) {
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
                Sleep(10);
                mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            }
        }
    }
}

unsigned int AOBScan(HANDLE processHandle, const char* pattern, const char* mask, unsigned int scanSize) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    uintptr_t currentAddress = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    std::vector<std::pair<uintptr_t, size_t>> regions;

    while (currentAddress < (uintptr_t)sysInfo.lpMaximumApplicationAddress) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(processHandle, (LPCVOID)currentAddress, &mbi, sizeof(mbi))) {
            if (mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_EXECUTE_READWRITE))) {
                regions.push_back({currentAddress, mbi.RegionSize});
            }
            currentAddress += mbi.RegionSize;
        } else {
            currentAddress += 0x1000;
        }
    }

    for (const auto& region : regions) {
        std::vector<char> buffer(region.second);
        SIZE_T bytesRead;
        if (ReadProcessMemory(processHandle, (LPCVOID)region.first, buffer.data(), region.second, &bytesRead)) {
            for (size_t i = 0; i < bytesRead - strlen(mask); ++i) {
                bool found = true;
                for (size_t j = 0; j < strlen(mask); ++j) {
                    if (mask[j] != '?' && buffer[i + j] != pattern[j]) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    D3D11Renderer::Log("AOBScan: Match at 0x" + std::to_string(region.first + i));
                    return static_cast<unsigned int>(region.first + i);
                }
            }
        }
    }
    D3D11Renderer::Log("AOBScan: No match found");
    return 0;
}

uintptr_t AutoOffset(const char* pattern, const char* mask, int offset) {
    HANDLE processHandle = GetCurrentProcess();
    unsigned int address = AOBScan(processHandle, pattern, mask, 0x100000);
    if (address) {
        uintptr_t result = address + offset;
        D3D11Renderer::Log("AutoOffset: Found at 0x" + std::to_string(result));
        CloseHandle(processHandle);
        return result;
    }
    D3D11Renderer::Log("AutoOffset: Not found");
    CloseHandle(processHandle);
    return 0;
}

HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_Renderer) {
        g_Renderer = std::make_unique<D3D11Renderer>();
        if (!g_Renderer->Initialize(pSwapChain)) {
            D3D11Renderer::Log("Рендер не завёлся!");
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }
    g_Renderer->m_pContext->OMSetRenderTargets(1, &g_Renderer->m_pRenderTargetView, nullptr);

    GameObjectManager* gom = nullptr;
    BasePlayer* localPlayer = nullptr;
    Camera* mainCamera = nullptr;

    static const std::vector<std::pair<const char*, const char*>> patterns = {
        {"\x48\x83\xEC\x28\x48\x8B\x05", "xxxxxxx"},
        {"\x48\x89\x5C\x24\x08\x48\x89", "xxxxxxx"}
    };
    for (const auto& p : patterns) {
        uintptr_t gomAddress = AutoOffset(p.first, p.second, 0x7);
        if (gomAddress) {
            gom = reinterpret_cast<GameObjectManager*>(gomAddress);
            break;
        }
    }

    if (gom) {
        localPlayer = gom->GetPlayerByIndex(0);
        mainCamera = gom->GetMainCamera();
        std::thread espThread(DrawESP, localPlayer, mainCamera, gom);
        std::thread aimbotThread(Aimbot, localPlayer, gom->GetPlayerByIndex(1), mainCamera);
        std::thread triggerThread(Triggerbot, localPlayer, mainCamera, gom);
        espThread.join();
        aimbotThread.join();
        triggerThread.join();
    }

    g_Renderer->RenderImGui();
    return oPresent(pSwapChain, SyncInterval, Flags);
}

void SetupHook() {
    HMODULE hDXGI = GetModuleHandleA("dxgi.dll");
    if (!hDXGI) {
        D3D11Renderer::Log("Где dxgi.dll, бери его!");
        return;
    }
    oPresent = (Present_t)GetProcAddress(hDXGI, "Present");
    if (!oPresent) {
        D3D11Renderer::Log("Present не найден!");
        return;
    }
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    if (DetourAttach(&(PVOID&)oPresent, HookedPresent) != NO_ERROR) {
        D3D11Renderer::Log("Detour не прицепился!");
        return;
    }
    if (DetourTransactionCommit() != NO_ERROR) {
        D3D11Renderer::Log("Detour транзакция не прошла!");
        return;
    }
    D3D11Renderer::Log("Хук на Present готов!");
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    SetupHook();
    while (true) Sleep(1);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        D3D11Renderer::Log("Читик запустился!");
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        g_Renderer.reset();
        if (oPresent) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach(&(PVOID&)oPresent, HookedPresent);
            DetourTransactionCommit();
            oPresent = nullptr;
        }
    }
    return TRUE;
}