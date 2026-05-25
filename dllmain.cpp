#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>

// --- CS2 DUMPER VERİLERİ (24 MAYIS 2026) ---
namespace Offsets {
    constexpr std::ptrdiff_t dwEntityList = 0x24E44E0;
    constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x2069800;
    constexpr std::ptrdiff_t dwViewMatrix = 0x206E880;
}

namespace ClientDll {
    constexpr std::ptrdiff_t m_iHealth = 0x324;
    constexpr std::ptrdiff_t m_iTeamNum = 0x3C3;
    constexpr std::ptrdiff_t m_vOldOrigin = 0x127C;
    constexpr std::ptrdiff_t m_pGameSceneNode = 0x308;
}

// Matematik Yapıları
struct Vector3 {
    float x, y, z;
};

struct ViewMatrix {
    float matrix[4][4];
};

// 3D Harita Koordinatını 2D Ekrana Çevirme (World To Screen)
bool WorldToScreen(Vector3 pos, Vector3& screen, ViewMatrix matrix, int width, int height) {
    float _x = matrix.matrix[0][0] * pos.x + matrix.matrix[0][1] * pos.y + matrix.matrix[0][2] * pos.z + matrix.matrix[0][3];
    float _y = matrix.matrix[1][0] * pos.x + matrix.matrix[1][1] * pos.y + matrix.matrix[1][2] * pos.z + matrix.matrix[1][3];
    float w = matrix.matrix[3][0] * pos.x + matrix.matrix[3][1] * pos.y + matrix.matrix[3][2] * pos.z + matrix.matrix[3][3];

    if (w < 0.01f) return false;

    float invw = 1.0f / w;
    _x *= invw;
    _y *= invw;

    float x = width / 2.0f;
    float y = height / 2.0f;

    x += 0.5f * _x * width + 0.5f;
    y -= 0.5f * _y * height + 0.5f;

    screen.x = x;
    screen.y = y;
    return true;
}

// GDI Kutu Çizim Fonksiyonu
void DrawBorderBox(HDC hdc, int x, int y, int w, int h, int thickness, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT rectTop = { x, y, x + w, y + thickness };
    FillRect(hdc, &rectTop, brush);
    RECT rectLeft = { x, y, x + thickness, y + h };
    FillRect(hdc, &rectLeft, brush);
    RECT rectRight = { x + w - thickness, y, x + w, y + h };
    FillRect(hdc, &rectRight, brush);
    RECT rectBottom = { x, y + h - thickness, x + w, y + h };
    FillRect(hdc, &rectBottom, brush);
    DeleteObject(brush);
}

// Ana Hile Fonksiyonu
void CheatThread(HMODULE hModule) {
    // Bilgilendirme Konsolu Açalım
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    std::cout << "[Axion] Internal ESP Yuklendi!" << std::endl;
    std::cout << "[Axion] Kapatmak icin hile icindeyken END tusuna basin." << std::endl;

    uintptr_t clientModule = (uintptr_t)GetModuleHandleA("client.dll");
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    HDC hdc = GetDC(NULL);

    while (!GetAsyncKeyState(VK_END)) {
        if (!clientModule) {
            clientModule = (uintptr_t)GetModuleHandleA("client.dll");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        uintptr_t localPlayerPawn = *(uintptr_t*)(clientModule + Offsets::dwLocalPlayerPawn);
        if (!localPlayerPawn) continue;

        int localTeam = *(int*)(localPlayerPawn + ClientDll::m_iTeamNum);
        ViewMatrix viewMatrix = *(ViewMatrix*)(clientModule + Offsets::dwViewMatrix);
        uintptr_t entityList = *(uintptr_t*)(clientModule + Offsets::dwEntityList);
        if (!entityList) continue;

        // Oyuncu Listesini Tara
        for (int i = 1; i < 64; i++) {
            uintptr_t listEntry = *(uintptr_t*)(entityList + ((8 * (i & 0x7FFF) >> 9) + 16));
            if (!listEntry) continue;

            uintptr_t playerPawn = *(uintptr_t*)(listEntry + 120 * (i & 0x1FF));
            if (!playerPawn || playerPawn == localPlayerPawn) continue;

            int health = *(int*)(playerPawn + ClientDll::m_iHealth);
            if (health <= 0 || health > 100) continue;

            int team = *(int*)(playerPawn + ClientDll::m_iTeamNum);
            if (team == localTeam) continue; // Dostları çizme

            // Pozisyonları Al
            Vector3 feetPos = *(Vector3*)(playerPawn + ClientDll::m_vOldOrigin);
            uintptr_t gameSceneNode = *(uintptr_t*)(playerPawn + ClientDll::m_pGameSceneNode);
            if (!gameSceneNode) continue;
            
            Vector3 headPos = *(Vector3*)(gameSceneNode + 0x80); // GameSceneNode abs origin (Kafa Hizası)

            Vector3 screenFeet, screenHead;
            if (WorldToScreen(feetPos, screenFeet, viewMatrix, screenWidth, screenHeight) &&
                WorldToScreen(headPos, screenHead, viewMatrix, screenWidth, screenHeight)) {

                int height = std::abs(static_cast<int>(screenFeet.y - screenHead.y));
                int width = height / 2;
                int x = static_cast<int>(screenHead.x) - (width / 2);
                int y = static_cast<int>(screenHead.y);

                // Düşmanlara Kırmızı Kutu Çiz
                DrawBorderBox(hdc, x, y, width, height, 2, RGB(255, 0, 0));
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS Çizim Hızı
    }

    std::cout << "[Axion] Kapatiliyor..." << std::endl;
    ReleaseDC(NULL, hdc);
    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == SIGNAL_LN_ENTRY_PROCESSING || ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CloseHandle(CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)CheatThread, hModule, 0, nullptr));
    }
    return TRUE;
}
