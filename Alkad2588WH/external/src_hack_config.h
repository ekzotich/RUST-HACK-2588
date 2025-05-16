#include "hack_config.h"
#include "renderer.h"

void HackConfig::Save() {
    std::ofstream file("config.bin", std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<char*>(this), sizeof(HackConfig));
        file.close();
        D3D11Renderer::Log("Конфиг сохранён!");
    }
}

void HackConfig::Load() {
    std::ifstream file("config.bin", std::ios::binary);
    if (file.is_open()) {
        file.read(reinterpret_cast<char*>(this), sizeof(HackConfig));
        file.close();
        D3D11Renderer::Log("Конфиг загружен!");
    }
}