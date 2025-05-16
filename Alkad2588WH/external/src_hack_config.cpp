#pragma once
#include <string>
#include <fstream>

class HackConfig {
public:
    bool g_bESP = false, g_bAimbot = false, g_bTriggerbot = false;
    float g_fAimbotDistance = 150.0f, g_fAimbotSensitivity = 1.0f;
    float g_fESPBox[3] = {1.0f, 0.0f, 0.0f};

    static HackConfig& GetInstance() {
        static HackConfig instance;
        return instance;
    }
    void Save();
    void Load();

private:
    HackConfig() { Load(); }
};