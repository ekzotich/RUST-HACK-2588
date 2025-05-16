#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <memory>
#include <mutex>
#include <string>
#include <fstream>
#include <chrono>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

struct Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& other) const { return {x + other.x, y + other.y, z + other.z}; }
    Vector3 operator*(float scalar) const { return {x * scalar, y * scalar, z * scalar}; }
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

struct GameObjectManager {
public:
    virtual uintptr_t GetActiveObjects() = 0;
    virtual int GetPlayerCount() = 0;
    virtual BasePlayer* GetPlayerByIndex(int index) = 0;
    virtual Camera* GetMainCamera() = 0;
    virtual int GetObjectCount() = 0;
    virtual ~GameObjectManager() = default;
};

class D3D11Renderer {
private:
    ID3D11Device* m_pDevice = nullptr;
    ID3D11DeviceContext* m_pContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_pRenderTargetView = nullptr;
    std::unique_ptr<ID3D11VertexShader> m_pVertexShader;
    std::unique_ptr<ID3D11PixelShader> m_pPixelShader;
    std::unique_ptr<ID3D11InputLayout> m_pInputLayout;
    std::unique_ptr<ID3D11Buffer> m_pVertexBuffer;
    std::mutex m_renderMutex;

    static std::mutex m_logMutex;
    struct Vertex {
        FLOAT x, y, z;
        FLOAT r, g, b, a;
    };

public:
    static void Log(const std::string& msg);
    bool Initialize(IDXGISwapChain* pSwapChain);
    void DrawLine(float x1, float y1, float x2, float y2, float r, float g, float b);
    void DrawRect(float x1, float y1, float x2, float y2, float r, float g, float b);
    void RenderImGui();
    ~D3D11Renderer();
};