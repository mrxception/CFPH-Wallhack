#include <Windows.h>
#include <d3d9.h>
#include <time.h>
#include <string>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui_impl_win32.h"
#include <random>

#pragma comment(lib, "d3d9.lib")
#include "detours.h"
#if defined _M_X64
#pragma comment(lib, "detours.X64/detours.lib")
#elif defined _M_IX86
#pragma comment(lib, "detours.X86/detours.lib")
#endif

typedef HRESULT(APIENTRY* EndScene_t)(IDirect3DDevice9*);
typedef HRESULT(APIENTRY* DrawIndexedPrimitive_t)(IDirect3DDevice9*, D3DPRIMITIVETYPE, INT, UINT, UINT, UINT, UINT);
typedef HRESULT(APIENTRY* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT(APIENTRY* SetStreamSource_t)(IDirect3DDevice9*, UINT, IDirect3DVertexBuffer9*, UINT, UINT);

static EndScene_t oEndScene = nullptr;
static DrawIndexedPrimitive_t oDrawIndexedPrimitive = nullptr;
static Reset_t oReset = nullptr;
static SetStreamSource_t oSetStreamSource = nullptr;

static HWND game_hwnd = nullptr;
static WNDPROC oWndProc = nullptr;
static bool wallhack = true;
static bool show_menu = true;
static UINT current_stride = 0;
static bool hooks_initialized = false;

static bool crosshair_expanded = true;
static bool visuals_expanded = true;
static bool developer_expanded = true;

static int wallhack_mode = 0;
static const char* wallhack_modes[] = { "Normal", "Wireframe", "Chams" };

static bool crosshair_enabled = false;
static int crosshair_type = 0;
static const char* crosshair_types[] = { "Cross", "Dot", "Circle", "T-Shape" };
static float crosshair_color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
static float crosshair_size = 10.0f;
static float crosshair_thickness = 1.0f;

static ImVec4 gold_color = ImVec4(0.831f, 0.686f, 0.216f, 1.0f);
static ImVec4 window_bg_color = ImVec4(0.070f, 0.070f, 0.070f, 0.94f);
static ImVec4 button_color = ImVec4(0.1f, 0.1f, 0.1f, 0.8f);
static ImVec4 button_hovered = ImVec4(0.15f, 0.15f, 0.15f, 0.9f);
static ImVec4 button_active = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);

std::string ObfuscateString(const std::string& input) {
    std::string result = input;
    const char key = 0x5F;
    for (char& c : result) {
        c ^= key;
    }
    return result;
}

std::string DeobfuscateString(const std::string& input) {
    return ObfuscateString(input);
}

static const std::string obfuscated_title = ObfuscateString("MRX Crossfire Philippines");

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (show_menu)
    {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
            return true;

        if (ImGui::GetIO().WantCaptureMouse &&
            (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN ||
                msg == WM_RBUTTONUP || msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
                msg == WM_MOUSEWHEEL || msg == WM_MOUSEMOVE))
        {
            return true;
        }

        if (ImGui::GetIO().WantCaptureKeyboard &&
            (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_CHAR))
        {
            return true;
        }
    }

    return CallWindowProc(oWndProc, hWnd, msg, wParam, lParam);
}

void SetCustomImGuiStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.15f, 0.95f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.08f, 0.08f, 0.15f, 0.95f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.15f, 0.95f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.32f, 1.0f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.22f, 0.8f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.28f, 0.8f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.25f, 0.35f, 0.8f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.20f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.17f, 0.17f, 0.27f, 1.0f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.20f, 0.8f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.40f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.33f, 0.33f, 0.50f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.60f, 1.0f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.90f, 0.92f, 0.84f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.15f, 0.35f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.25f, 0.45f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.65f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.15f, 0.15f, 0.30f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.25f, 0.40f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.35f, 0.50f, 1.0f);

    style.WindowPadding = ImVec2(6.0f, 6.0f);
    style.FramePadding = ImVec2(4.0f, 2.0f);
    style.ItemSpacing = ImVec2(6.0f, 3.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
}



std::string GetCurrentTimeString()
{
    time_t now = time(0);
    struct tm timeinfo;
    char buffer[80];
    localtime_s(&timeinfo, &now);
    strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
    return std::string(buffer);
}

bool RenderMenuItem(const char* label, bool& expanded, bool is_selected)
{
    float width = ImGui::GetContentRegionAvail().x;
    std::string id = std::string("##") + label;
    ImGui::PushStyleColor(ImGuiCol_Text, gold_color);
    if (is_selected)
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.2f, 0.2f, 0.8f));

    bool clicked = ImGui::Selectable(id.c_str(), is_selected, 0, ImVec2(width, 0));
    if (is_selected)
        ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);
    ImGui::SetCursorPosX(10.0f);
    ImGui::Text("%s", label);
    ImGui::SameLine();
    float markWidth = ImGui::CalcTextSize(expanded ? "[-]" : "[+]").x;
    ImGui::SetCursorPosX(width - markWidth);
    ImGui::Text("%s", expanded ? "[-]" : "[+]");
    ImGui::PopStyleColor();

    return clicked;
}

void RenderWallhackSettings()
{
    ImGui::Indent(10.0f);
    static int current_mode = wallhack_mode;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

    bool changed = false;
    if (ImGui::IsWindowFocused())
    {
        if (GetAsyncKeyState(VK_UP) & 1)
        {
            current_mode = (current_mode > 0) ? current_mode - 1 : IM_ARRAYSIZE(wallhack_modes) - 1;
            changed = true;
        }
        else if (GetAsyncKeyState(VK_DOWN) & 1)
        {
            current_mode = (current_mode + 1) % IM_ARRAYSIZE(wallhack_modes);
            changed = true;
        }
    }

    for (int i = 0; i < IM_ARRAYSIZE(wallhack_modes); i++)
    {
        ImGui::BeginGroup();
        bool selected = (current_mode == i);
        if (ImGui::Selectable(wallhack_modes[i], &selected, 0, ImVec2(100, 0)))
        {
            current_mode = i;
            changed = true;
        }
        ImGui::EndGroup();
    }

    ImGui::PopStyleColor();
    ImGui::Unindent(10.0f);
    if (changed)
        wallhack_mode = current_mode;
}

void RenderCrosshairSettings()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));

    ImGui::BeginGroup();
    ImGui::Checkbox("Enable Crosshair", &crosshair_enabled);
    ImGui::Spacing();
    ImGui::Text("Crosshair Style");
    ImGui::Combo("Type", &crosshair_type, crosshair_types, IM_ARRAYSIZE(crosshair_types));
    ImGui::Spacing();
    ImGui::Text("Appearance");
    ImGui::ColorEdit4("Color", crosshair_color, ImGuiColorEditFlags_AlphaBar);
    ImGui::PushItemWidth(130.0f);
    ImGui::SliderFloat("Size", &crosshair_size, 5.0f, 50.0f, "%.1f");
    ImGui::SliderFloat("Thickness", &crosshair_thickness, 1.0f, 10.0f, "%.1f");
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::PopStyleColor();
}

void DrawCrosshair(ImDrawList* draw_list, ImVec2 screen_center)
{
    if (!crosshair_enabled)
        return;

    ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(crosshair_color[0], crosshair_color[1], crosshair_color[2], crosshair_color[3]));
    float size = crosshair_size;
    float thickness = crosshair_thickness;

    switch (crosshair_type)
    {
    case 0:
        draw_list->AddLine(ImVec2(screen_center.x - size, screen_center.y), ImVec2(screen_center.x + size, screen_center.y), color, thickness);
        draw_list->AddLine(ImVec2(screen_center.x, screen_center.y - size), ImVec2(screen_center.x, screen_center.y + size), color, thickness);
        break;
    case 1:
        draw_list->AddCircleFilled(screen_center, size / 2, color);
        break;
    case 2:
        draw_list->AddCircle(screen_center, size, color, 0, thickness);
        break;
    case 3:
        draw_list->AddLine(ImVec2(screen_center.x, screen_center.y - size), ImVec2(screen_center.x, screen_center.y), color, thickness);
        draw_list->AddLine(ImVec2(screen_center.x - size, screen_center.y), ImVec2(screen_center.x + size, screen_center.y), color, thickness);
        break;
    }
}

void RenderMenu()
{
    if (!show_menu)
        return;

    ImGui::SetNextWindowSize(ImVec2(220, 415));
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
    ImGui::Begin("##MainWindow", &show_menu,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    float windowWidth = ImGui::GetWindowWidth();
    std::string title = DeobfuscateString(obfuscated_title);
    float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
    ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
    ImGui::TextColored(gold_color, "%s", title.c_str());
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
    ImGui::Separator();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);

    static int current_menu_item = 0;
    const int num_menu_items = 3;

    if (ImGui::IsWindowFocused())
    {
        if (GetAsyncKeyState(VK_UP) & 1)
            current_menu_item = (current_menu_item > 0) ? current_menu_item - 1 : num_menu_items - 1;
        else if (GetAsyncKeyState(VK_DOWN) & 1)
            current_menu_item = (current_menu_item + 1) % num_menu_items;
        if ((GetAsyncKeyState(VK_LEFT) & 1) || (GetAsyncKeyState(VK_RIGHT) & 1) ||
            (GetAsyncKeyState(VK_RETURN) & 1) || (GetAsyncKeyState(VK_SPACE) & 1))
        {
            if (current_menu_item == 0)
                crosshair_expanded = !crosshair_expanded;
            else if (current_menu_item == 1)
                visuals_expanded = !visuals_expanded;
            else if (current_menu_item == 2)
                developer_expanded = !developer_expanded;
        }
    }

    ImGui::Spacing();
    bool crosshair_clicked = RenderMenuItem("Crosshair", crosshair_expanded, current_menu_item == 0);
    if (crosshair_clicked)
    {
        crosshair_expanded = !crosshair_expanded;
        current_menu_item = 0;
    }

    if (crosshair_expanded)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::Indent(10.0f);
        ImGui::Spacing();
        RenderCrosshairSettings();
        ImGui::Spacing();
        ImGui::Unindent(10.0f);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    bool visual_clicked = RenderMenuItem("Visual", visuals_expanded, current_menu_item == 1);
    if (visual_clicked)
    {
        visuals_expanded = !visuals_expanded;
        current_menu_item = 1;
    }

    if (visuals_expanded)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::Indent(10.0f);
        ImGui::Spacing();
        ImGui::Checkbox("Enable Wallhack", &wallhack);
        RenderWallhackSettings();
        ImGui::Unindent(10.0f);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    bool developer_clicked = RenderMenuItem("Developer", developer_expanded, current_menu_item == 2);
    if (developer_clicked)
    {
        developer_expanded = !developer_expanded;
        current_menu_item = 2;
    }

    if (developer_expanded)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
        ImGui::Indent(10.0f);
        ImGui::Spacing();
        ImGui::Text("Developer: %s", DeobfuscateString(ObfuscateString("MRX")).c_str());
        ImGui::Text("Game: %s", DeobfuscateString(ObfuscateString("Crossfire PH")).c_str());
        ImGui::Text("Version: %s", DeobfuscateString(ObfuscateString("1.2")).c_str());
        ImGui::Text("Build Date: %s", DeobfuscateString(ObfuscateString("May 18, 2025")).c_str());
        ImGui::Unindent(10.0f);
        ImGui::PopStyleColor();
    }

    std::string timeStr = GetCurrentTimeString();
    float timeWidth = ImGui::CalcTextSize(timeStr.c_str()).x;
    float availHeight = ImGui::GetWindowHeight();
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosY(availHeight - 32);
    ImGui::Separator();
    ImGui::SetCursorPos(ImVec2((availWidth - timeWidth) * 0.5f, availHeight - 25));
    ImGui::Text("%s", timeStr.c_str());

    ImGui::End();
}

static HRESULT APIENTRY hkDrawIndexedPrimitive(IDirect3DDevice9* pDevice, D3DPRIMITIVETYPE Type,
    INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
    if (!wallhack || (current_stride != 36 && current_stride != 32 && current_stride != 44 && current_stride != 68 && current_stride != 40 && current_stride != 56))
        return oDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

    DWORD originalZEnable, originalZWrite, originalZFunc, originalFillMode;
    DWORD originalAlphaBlendEnable, originalSrcBlend, originalDestBlend;
    DWORD originalLighting, originalSpecularEnable;

    pDevice->GetRenderState(D3DRS_ZENABLE, &originalZEnable);
    pDevice->GetRenderState(D3DRS_ZWRITEENABLE, &originalZWrite);
    pDevice->GetRenderState(D3DRS_ZFUNC, &originalZFunc);
    pDevice->GetRenderState(D3DRS_FILLMODE, &originalFillMode);
    pDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &originalAlphaBlendEnable);
    pDevice->GetRenderState(D3DRS_SRCBLEND, &originalSrcBlend);
    pDevice->GetRenderState(D3DRS_DESTBLEND, &originalDestBlend);
    pDevice->GetRenderState(D3DRS_LIGHTING, &originalLighting);
    pDevice->GetRenderState(D3DRS_SPECULARENABLE, &originalSpecularEnable);

    pDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    pDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    pDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);

    HRESULT result = S_OK;
    switch (wallhack_mode)
    {
    case 0:
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
        pDevice->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        result = oDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        pDevice->SetRenderState(D3DRS_ZENABLE, originalZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, originalZWrite);
        pDevice->SetRenderState(D3DRS_ZFUNC, originalZFunc);
        pDevice->SetRenderState(D3DRS_LIGHTING, originalLighting);
        pDevice->SetRenderState(D3DRS_SPECULARENABLE, originalSpecularEnable);
        break;

    case 1:
        pDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
        result = oDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        pDevice->SetRenderState(D3DRS_FILLMODE, originalFillMode);
        break;

    case 2:
        pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(255, 0, 0, 150));
        pDevice->SetTexture(0, NULL);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
        oDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        pDevice->SetRenderState(D3DRS_ZENABLE, originalZEnable);
        pDevice->SetRenderState(D3DRS_ZWRITEENABLE, originalZWrite);
        pDevice->SetRenderState(D3DRS_ZFUNC, originalZFunc);
        pDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(0, 0, 255, 255));
        result = oDrawIndexedPrimitive(pDevice, Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
        pDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        break;
    }

    return result;
}

static HRESULT APIENTRY hkEndScene(IDirect3DDevice9* pDevice)
{
    static bool init = false;
    if (!init && pDevice)
    {
        D3DDEVICE_CREATION_PARAMETERS params;
        pDevice->GetCreationParameters(&params);
        game_hwnd = params.hFocusWindow;
        oWndProc = (WNDPROC)SetWindowLongPtr(game_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(game_hwnd);
        ImGui_ImplDX9_Init(pDevice);
        SetCustomImGuiStyle();
        init = true;
    }

    if (GetAsyncKeyState(VK_INSERT) & 1)
        show_menu = !show_menu;

    if (init)
    {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        SetCustomImGuiStyle();
        RenderMenu();

        if (crosshair_enabled)
        {
            ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
            RECT rect;
            GetClientRect(game_hwnd, &rect);
            ImVec2 screen_center((rect.right - rect.left) / 2.0f, (rect.bottom - rect.top) / 2.0f);
            DrawCrosshair(draw_list, screen_center);
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return oEndScene(pDevice);
}

static HRESULT APIENTRY hkReset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        HRESULT hr = oReset(pDevice, pPresentationParameters);
        ImGui_ImplDX9_CreateDeviceObjects();
        return hr;
    }
    return oReset(pDevice, pPresentationParameters);
}

static HRESULT APIENTRY hkSetStreamSource(IDirect3DDevice9* pDevice, UINT StreamNumber,
    IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
{
    if (StreamNumber == 0)
        current_stride = Stride;

    return oSetStreamSource(pDevice, StreamNumber, pStreamData, OffsetInBytes, Stride);
}

void* GetVTableFunction(IDirect3DDevice9* pDevice, int index)
{
    void** vTable = *reinterpret_cast<void***>(pDevice);
    return vTable[index];
}

static void InitializeHooks()
{
    if (hooks_initialized) return;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 500);
    Sleep(dis(gen));

    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!pD3D) return;

    D3DPRESENT_PARAMETERS d3dpp = { 0 };
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = NULL;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;

    IDirect3DDevice9* pDummyDevice = nullptr;
    HWND hWnd = CreateWindowA("STATIC", NULL, WS_OVERLAPPED, 0, 0, 1, 1, NULL, NULL, NULL, NULL);
    if (!hWnd) return;

    HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED, &d3dpp, &pDummyDevice);
    if (FAILED(hr)) return;

    oEndScene = (EndScene_t)GetVTableFunction(pDummyDevice, 42);
    oDrawIndexedPrimitive = (DrawIndexedPrimitive_t)GetVTableFunction(pDummyDevice, 82);
    oReset = (Reset_t)GetVTableFunction(pDummyDevice, 16);
    oSetStreamSource = (SetStreamSource_t)GetVTableFunction(pDummyDevice, 100);

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&)oEndScene, hkEndScene);
    DetourAttach(&(PVOID&)oDrawIndexedPrimitive, hkDrawIndexedPrimitive);
    DetourAttach(&(PVOID&)oReset, hkReset);
    DetourAttach(&(PVOID&)oSetStreamSource, hkSetStreamSource);
    DetourTransactionCommit();

    pDummyDevice->Release();
    pD3D->Release();
    DestroyWindow(hWnd);
    hooks_initialized = true;
}

static void CleanupHooks()
{
    if (!hooks_initialized) return;

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourDetach(&(PVOID&)oEndScene, hkEndScene);
    DetourDetach(&(PVOID&)oDrawIndexedPrimitive, hkDrawIndexedPrimitive);
    DetourDetach(&(PVOID&)oReset, hkReset);
    DetourDetach(&(PVOID&)oSetStreamSource, hkSetStreamSource);
    DetourTransactionCommit();

    if (game_hwnd && oWndProc)
        SetWindowLongPtr(game_hwnd, GWLP_WNDPROC, (LONG_PTR)oWndProc);

    if (ImGui::GetCurrentContext())
    {
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    hooks_initialized = false;
}

extern "C" __declspec(dllexport) int JYT(int code, WPARAM wParam, LPARAM lParam) {
    if (code >= 0)
    {
        static bool initialized = false;
        if (!initialized)
        {
            InitializeHooks();
            initialized = true;
        }
    }
    return CallNextHookEx(NULL, code, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    if (reason == DLL_PROCESS_DETACH)
    {
        CleanupHooks();
    }
    return TRUE;
}
