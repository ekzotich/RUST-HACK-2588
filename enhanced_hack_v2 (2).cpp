#define _HAS_PCH 0
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <detours.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "detours.lib")
#pragma comment(lib, "imgui.lib")

struct Vector3 {
    float x, y, z;
};

struct Vector2 {
    float x, y;
};

struct BasePlayer {
    virtual Vector3 GetPosition() = 0;
    virtual Vector3 GetHeadPosition() = 0;
    virtual int GetTeam() = 0;
    virtual bool IsLocalPlayer() = 0;
    virtual float GetHealth() = 0;
};

struct Camera {
    virtual Vector3 GetPosition() = 0;
    virtual Vector3 WorldToScreen(Vector3 position) = 0;
};

struct GameObject {
    virtual Vector3 GetPosition() = 0;
    virtual char* GetName() = 0;
};

class GameObjectManager {
public:
    virtual uintptr_t GetActiveObjects() = 0;
    virtual int GetPlayerCount() = 0;
    virtual BasePlayer* GetPlayerByIndex(int index) = 0;
    virtual Camera* GetMainCamera() = 0;
    virtual GameObject* GetObjectByIndex(int index) = 0;
    virtual int GetObjectCount() = 0;
    virtual ~GameObjectManager() = default;
};

bool g_bESP = false, g_bAimbot = false, g_bTriggerbot = false;
float g_fAimbotDistance = 150.0f, g_fAimbotSensitivity = 1.0f;
float g_fESPBoxRed = 1.0f, g_fESPBoxGreen = 0.0f, g_fESPBoxBlue = 0.0f;
float g_fObjectMarkerRed = 0.0f, g_fObjectMarkerGreen = 1.0f, g_fObjectMarkerBlue = 0.0f;
ID3D11Device* g_pDevice = nullptr;
ID3D11DeviceContext* g_pContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ImGuiContext* g_pImGuiContext = nullptr;

typedef HRESULT(WINAPI* Present_t)(IDXGISwapChain*, UINT, UINT);
Present_t oPresent = nullptr;

struct Vertex {
    FLOAT x, y, z;
    FLOAT r, g, b, a;
};

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;

void Log(const std::string& msg) {
    std::ofstream logfile("hack_log.txt", std::ios::app);
    logfile << msg << std::endl;
    logfile.close();
}

unsigned int AOBScan(HANDLE processHandle, const char* pattern, const char* mask, unsigned int scanSize) {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    uintptr_t currentAddress = (uintptr_t)sysInfo.lpMinimumApplicationAddress;
    while (currentAddress < (uintptr_t)sysInfo.lpMaximumApplicationAddress) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(processHandle, (LPCVOID)currentAddress, &mbi, sizeof(mbi))) {
            currentAddress += 0x1000;
            continue;
        }
        if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS) {
            currentAddress += mbi.RegionSize;
            continue;
        }
        std::vector<char> buffer(mbi.RegionSize);
        SIZE_T bytesRead;
        if (ReadProcessMemory(processHandle, (LPCVOID)currentAddress, buffer.data(), mbi.RegionSize, &bytesRead)) {
            for (size_t i = 0; i < bytesRead - strlen(mask); ++i) {
                bool found = true;
                for (size_t j = 0; j < strlen(mask); ++j) {
                    if (mask[j] != '?' && buffer[i + j] != pattern[j]) {
                        found = false;
                        break;
                    }
                }
                if (found) {
                    Log("AOBScan: Match at 0x" + std::to_string(currentAddress + i));
                    return (unsigned int)(currentAddress + i);
                }
            }
        }
        currentAddress += mbi.RegionSize;
    }
    Log("AOBScan: No match found");
    return 0;
}

uintptr_t AutoOffset(const char* pattern, const char* mask, int offset) {
    HANDLE processHandle = GetCurrentProcess();
    unsigned int address = AOBScan(processHandle, pattern, mask, 0x1000);
    if (address) {
        uintptr_t result = address + offset;
        Log("AutoOffset: Found at 0x" + std::to_string(result));
        CloseHandle(processHandle);
        return result;
    }
    Log("AutoOffset: Not found");
    CloseHandle(processHandle);
    return 0;
}

bool InitD3D11Resources() {
    const char* vertexShaderSrc = R"(
        struct VS_INPUT { float3 pos : POSITION; float4 color : COLOR; };
        struct PS_INPUT { float4 pos : SV_POSITION; float4 color : COLOR; };
        PS_INPUT main(VS_INPUT input) { PS_INPUT output; output.pos = float4(input.pos, 1.0f); output.color = input.color; return output; }
    )";
    const char* pixelShaderSrc = R"(
        struct PS_INPUT { float4 pos : SV_POSITION; float4 color : COLOR; };
        float4 main(PS_INPUT input) : SV_TARGET { return input.color; }
    )";

    ID3DBlob* pVSBlob = nullptr, *pPSBlob = nullptr;
    HRESULT hr = D3DCompile(vertexShaderSrc, strlen(vertexShaderSrc), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &pVSBlob, nullptr);
    if (FAILED(hr)) return false;
    hr = g_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &g_pVertexShader);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }

    hr = D3DCompile(pixelShaderSrc, strlen(pixelShaderSrc), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &pPSBlob, nullptr);
    if (FAILED(hr)) { pVSBlob->Release(); return false; }
    hr = g_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &g_pPixelShader);
    if (FAILED(hr)) { pVSBlob->Release(); pPSBlob->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = g_pDevice->CreateInputLayout(layout, 2, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pInputLayout);
    pVSBlob->Release(); pPSBlob->Release();
    if (FAILED(hr)) return false;

    D3D11_BUFFER_DESC bd = { 0 };
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * 8;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = g_pDevice->CreateBuffer(&bd, nullptr, &g_pVertexBuffer);
    if (FAILED(hr)) return false;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(GetForegroundWindow());
    ImGui_ImplDX11_Init(g_pDevice, g_pContext);
    Log("D3D11 and ImGui initialized");
    return true;
}

void DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b) {
    D3D11_VIEWPORT viewport;
    g_pContext->RSGetViewports(1, &viewport);
    x1 = (x1 / viewport.Width) * 2.0f - 1.0f; y1 = -((y1 / viewport.Height) * 2.0f - 1.0f);
    x2 = (x2 / viewport.Width) * 2.0f - 1.0f; y2 = -((y2 / viewport.Height) * 2.0f - 1.0f);

    Vertex vertices[] = { {x1, y1, 0.0f, r, g, b, 1.0f}, {x2, y2, 0.0f, r, g, b, 1.0f} };
    D3D11_MAPPED_SUBRESOURCE ms;
    g_pContext->Map(g_pVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    memcpy(ms.pData, vertices, sizeof(vertices));
    g_pContext->Unmap(g_pVertexBuffer, 0);

    UINT stride = sizeof(Vertex), offset = 0;
    g_pContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
    g_pContext->IASetInputLayout(g_pInputLayout);
    g_pContext->VSSetShader(g_pVertexShader, nullptr, 0);
    g_pContext->PSSetShader(g_pPixelShader, nullptr, 0);
    g_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    g_pContext->Draw(2, 0);
}

void DrawRect(float x1, float y1, float x2, float y2, float r, float g, float b) {
    DrawLine(x1, y1, x2, y1, r, g, b);
    DrawLine(x2, y1, x2, y2, r, g, b);
    DrawLine(x2, y2, x1, y2, r, g, b);
    DrawLine(x1, y2, x1, y1, r, g, b);
}

void DrawESP(BasePlayer* localPlayer, Camera* camera, GameObjectManager* gom) {
    if (!g_bESP || !localPlayer || !camera || !gom || !g_pContext) return;

    for (int i = 0; i < gom->GetPlayerCount(); ++i) {
        BasePlayer* player = gom->GetPlayerByIndex(i);
        if (!player || player->IsLocalPlayer() || player->GetHealth() <= 0) continue;

        Vector3 pos = player->GetPosition();
        Vector3 headPos = player->GetHeadPosition();
        Vector3 screenPos = camera->WorldToScreen(pos);
        Vector3 screenHead = camera->WorldToScreen(headPos);

        if (screenPos.z > 0 && screenHead.z > 0) {
            float height = abs(screenHead.y - screenPos.y);
            float width = height * 0.5f;
            DrawRect(screenPos.x - width / 2, screenHead.y, screenPos.x + width / 2, screenPos.y, g_fESPBoxRed, g_fESPBoxGreen, g_fESPBoxBlue);
        }
    }

    for (int i = 0; i < gom->GetObjectCount(); ++i) {
        GameObject* obj = gom->GetObjectByIndex(i);
        if (!obj) continue;
        Vector3 pos = obj->GetPosition();
        Vector3 screenPos = camera->WorldToScreen(pos);
        if (screenPos.z > 0) {
            DrawRect(screenPos.x - 5, screenPos.y - 5, screenPos.x + 5, screenPos.y + 5, g_fObjectMarkerRed, g_fObjectMarkerGreen, g_fObjectMarkerBlue);
        }
    }
}

void Aimbot(BasePlayer* localPlayer, BasePlayer* target, Camera* camera) {
    if (!g_bAimbot || !localPlayer || !target || !camera) return;

    Vector3 targetHead = target->GetHeadPosition();
    Vector3 screenPos = camera->WorldToScreen(targetHead);
    if (screenPos.z > 0) {
        Vector2 targetScreen = {screenPos.x, screenPos.y};
        float distance = sqrt(pow(targetScreen.x - 960, 2) + pow(targetScreen.y - 540, 2));
        if (distance < g_fAimbotDistance) {
            float dx = (targetScreen.x - 960) * g_fAimbotSensitivity;
            float dy = (targetScreen.y - 540) * g_fAimbotSensitivity;
            mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(dx), static_cast<DWORD>(dy), 0, 0);
        }
    }
}

void Triggerbot(BasePlayer* localPlayer, Camera* camera, GameObjectManager* gom) {
    if (!g_bTriggerbot || !localPlayer || !camera || !gom) return;

    Vector3 localPos = localPlayer->GetPosition();
    for (int i = 0; i < gom->GetPlayerCount(); ++i) {
        BasePlayer* target = gom->GetPlayerByIndex(i);
        if (!target || target->IsLocalPlayer() || target->GetHealth() <= 0) continue;

        Vector3 targetPos = target->GetHeadPosition();
        Vector3 screenPos = camera->WorldToScreen(targetPos);
        if (screenPos.z > 0 && abs(screenPos.x - 960) < 30 && abs(screenPos.y - 540) < 30) {
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Sleep(1);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        }
    }
}

void RenderImGui() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Alkad 2588 Hack Menu", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::BeginTabBar("HackTabs")) {
        // WH Tab
        if (ImGui::BeginTabItem("WallHack")) {
            ImGui::Checkbox("Enable ESP", &g_bESP);
            ImGui::ColorEdit3("Player Box Color", (float*)&g_fESPBoxRed);
            ImGui::ColorEdit3("Object Marker Color", (float*)&g_fObjectMarkerRed);
            ImGui::EndTabItem();
        }
        // Aimbot Tab
        if (ImGui::BeginTabItem("Aimbot")) {
            ImGui::Checkbox("Enable Aimbot", &g_bAimbot);
            ImGui::SliderFloat("Max Distance", &g_fAimbotDistance, 50.0f, 300.0f, "%.0f px");
            ImGui::SliderFloat("Sensitivity", &g_fAimbotSensitivity, 0.1f, 2.0f, "%.1f");
            ImGui::Checkbox("Enable Triggerbot", &g_bTriggerbot);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

HRESULT WINAPI HookedPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!g_pDevice) {
        pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_pDevice);
        g_pDevice->GetImmediateContext(&g_pContext);
        g_pSwapChain = pSwapChain;

        ID3D11Texture2D* pBackBuffer = nullptr;
        pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
        g_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
        pBackBuffer->Release();

        if (!InitD3D11Resources()) Log("Failed to init D3D11 resources");
    }

    g_pContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    GameObjectManager* gom = nullptr;
    BasePlayer* localPlayer = nullptr;
    Camera* mainCamera = nullptr;

    const char* pattern = "\x48\x83\xEC\x28\x48\x8B\x05";
    const char* mask = "xxxxxxx";
    uintptr_t gomAddress = AutoOffset(pattern, mask, 0x7);
    if (gomAddress) gom = reinterpret_cast<GameObjectManager*>(gomAddress);

    if (gom) {
        localPlayer = gom->GetPlayerByIndex(0);
        mainCamera = gom->GetMainCamera();
        DrawESP(localPlayer, mainCamera, gom);
        Triggerbot(localPlayer, mainCamera, gom);
        Aimbot(localPlayer, gom->GetPlayerByIndex(1), mainCamera);
    }

    RenderImGui();
    return oPresent(pSwapChain, SyncInterval, Flags);
}

void SetupHook() {
    HMODULE hDXGI = GetModuleHandleA("dxgi.dll");
    if (hDXGI) {
        FARPROC pPresent = GetProcAddress(hDXGI, "Present");
        if (pPresent) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            oPresent = (Present_t)DetourFindFunction("dxgi.dll", "Present");
            DetourAttach(&(PVOID&)oPresent, HookedPresent);
            DetourTransactionCommit();
            Log("Hook on Present set");
        }
    }
}

DWORD WINAPI MainThread(LPVOID lpParam) {
    SetupHook();
    while (true) {
        Sleep(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        if (g_pVertexBuffer) g_pVertexBuffer->Release();
        if (g_pInputLayout) g_pInputLayout->Release();
        if (g_pVertexShader) g_pVertexShader->Release();
        if (g_pPixelShader) g_pPixelShader->Release();
        if (g_pRenderTargetView) g_pRenderTargetView->Release();
        if (g_pContext) g_pContext->Release();
        if (g_pDevice) g_pDevice->Release();
    }
    return TRUE;
}