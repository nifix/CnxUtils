#include "directory_config.h"
#include "ini_files.h"

#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>

namespace
{
constexpr UINT WMAPP_TRAY = WM_APP + 1;
constexpr UINT IDM_CONFIGURE_DIRECTORY = 100;
constexpr UINT IDM_BASE_FILE = 1000;
constexpr UINT IDM_EXIT = 9000;

std::wstring g_sourceDirectory;
NOTIFYICONDATAW g_trayIcon{};
UINT g_taskbarCreatedMessage = 0;
HANDLE g_singleInstanceMutex = nullptr;

// Demande un nouveau dossier à l'utilisateur, l'applique immédiatement puis
// tente de l'enregistrer pour les prochains lancements.
void configure_source_directory(HWND owner)
{
    std::wstring selectedDirectory;
    if (!choose_source_directory(owner, g_sourceDirectory, selectedDirectory))
        return;

    g_sourceDirectory = selectedDirectory;
    if (!save_source_directory(g_sourceDirectory))
    {
        MessageBoxW(owner,
            L"Le dossier est utilisé pour cette session, mais il n'a pas pu être enregistré.",
            L"CnxSwitchy", MB_OK | MB_ICONWARNING);
    }
}

// Construit le menu contextuel : le dossier configurable en tête, les fichiers
// .ini sélectionnables au milieu, puis la commande de fermeture.
HMENU build_menu(const std::vector<std::wstring>& files)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return nullptr;

    const std::wstring sourceLabel = L"Source : " + g_sourceDirectory;
    AppendMenuW(menu, MF_STRING, IDM_CONFIGURE_DIRECTORY, sourceLabel.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    UINT commandId = IDM_BASE_FILE;
    for (const auto& file : files)
        AppendMenuW(menu, MF_STRING, commandId++, file.c_str());

    if (files.empty())
        AppendMenuW(menu, MF_GRAYED | MF_STRING, 0, L"(No .ini files found)");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
    return menu;
}

// Affiche le menu à la position de la souris et exécute le choix retourné par
// TrackPopupMenu : configuration, activation d'un .ini ou fermeture.
void show_tray_menu(HWND window)
{
    const auto files = list_ini_files(g_sourceDirectory);
    HMENU menu = build_menu(files);
    if (!menu)
        return;

    POINT cursorPosition{};
    GetCursorPos(&cursorPosition);
    SetForegroundWindow(window);

    const UINT selection = TrackPopupMenu(menu,
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        cursorPosition.x, cursorPosition.y, 0, window, nullptr);

    if (selection == IDM_EXIT)
    {
        DestroyMenu(menu);
        DestroyWindow(window);
        return;
    }

    if (selection == IDM_CONFIGURE_DIRECTORY)
    {
        DestroyMenu(menu);
        configure_source_directory(window);
        return;
    }

    if (selection >= IDM_BASE_FILE && selection < IDM_EXIT)
    {
        const auto index = static_cast<size_t>(selection - IDM_BASE_FILE);
        if (index < files.size())
        {
            if (!activate_ini_file(g_sourceDirectory, files[index]))
            {
                MessageBoxW(window, L"Failed to update cnx.ini",
                    L"CnxSwitchy", MB_OK | MB_ICONERROR);
            }
            else
            {
                const std::wstring source = build_path(g_sourceDirectory, files[index]);
                g_trayIcon.uFlags = NIF_INFO;
                wcscpy_s(g_trayIcon.szInfoTitle, L"CnxSwitchy");
                wcsncpy_s(g_trayIcon.szInfo,
                    sizeof(g_trayIcon.szInfo) / sizeof(g_trayIcon.szInfo[0]),
                    source.c_str(), _TRUNCATE);
                Shell_NotifyIconW(NIM_MODIFY, &g_trayIcon);
                g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
            }
        }
    }

    DestroyMenu(menu);
}

// Reçoit les messages de la fenêtre invisible : clic sur l'icône, redémarrage
// de l'Explorateur Windows et destruction propre de l'application.
LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (g_taskbarCreatedMessage != 0 && message == g_taskbarCreatedMessage)
    {
        Shell_NotifyIconW(NIM_ADD, &g_trayIcon);
        return 0;
    }

    switch (message)
    {
    case WMAPP_TRAY:
        if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP)
            show_tray_menu(window);
        return 0;

    case WM_DESTROY:
        Shell_NotifyIconW(NIM_DELETE, &g_trayIcon);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

// Enregistre une classe Windows minimale et crée la fenêtre invisible qui sert
// uniquement de destinataire aux messages de l'icône de notification.
HWND create_hidden_window(HINSTANCE instance)
{
    constexpr wchar_t CLASS_NAME[] = L"CnxSwitchyWindowClass";

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.lpfnWndProc = window_procedure;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = CLASS_NAME;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);

    if (!RegisterClassExW(&windowClass))
        return nullptr;

    return CreateWindowExW(0, CLASS_NAME, L"CnxSwitchy", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, instance, nullptr);
}

// Prépare l'icône, son infobulle et le message de clic, puis l'ajoute à la zone
// de notification de Windows.
bool add_tray_icon(HINSTANCE instance, HWND window)
{
    g_trayIcon = {};
    g_trayIcon.cbSize = sizeof(g_trayIcon);
    g_trayIcon.hWnd = window;
    g_trayIcon.uID = 1;
    g_trayIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_trayIcon.uCallbackMessage = WMAPP_TRAY;

    auto icon = static_cast<HICON>(LoadImageW(instance, L"IDI_APP_ICON",
        IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
    if (!icon)
        icon = LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));

    g_trayIcon.hIcon = icon;
    wcscpy_s(g_trayIcon.szTip, L"CnxSwitchy");
    return Shell_NotifyIconW(NIM_ADD, &g_trayIcon) != FALSE;
}
}

// Point d'entrée de l'application : garantit une seule instance, restaure la
// configuration, initialise Windows/COM et exécute la boucle de messages.
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int)
{
    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, L"CnxSwitchy_Mutex");
    if (!g_singleInstanceMutex || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (g_singleInstanceMutex)
            CloseHandle(g_singleInstanceMutex);

        return 0;
    }

    g_sourceDirectory = load_source_directory();
    const HRESULT oleResult = OleInitialize(nullptr);

    HWND window = create_hidden_window(instance);
    if (!window)
    {
        CloseHandle(g_singleInstanceMutex);
        if (SUCCEEDED(oleResult))
            OleUninitialize();
        return 1;
    }

    ShowWindow(window, SW_HIDE);
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    add_tray_icon(instance, window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    CloseHandle(g_singleInstanceMutex);
    if (SUCCEEDED(oleResult))
        OleUninitialize();

    return 0;
}
