#include "directory_config.h"

#include <shlobj.h>

namespace
{
constexpr wchar_t DEFAULT_DIRECTORY[] = L"C:\\iSuiteExpert";
constexpr wchar_t REGISTRY_KEY[] = L"Software\\CnxUtils";
constexpr wchar_t REGISTRY_VALUE[] = L"SourceDirectory";

// Positionne le sélecteur Windows sur le dossier actuellement configuré dès
// que sa fenêtre est prête.
int CALLBACK browse_callback(HWND window, UINT message, LPARAM, LPARAM data)
{
    if (message == BFFM_INITIALIZED && data != 0)
        SendMessageW(window, BFFM_SETSELECTIONW, TRUE, data);

    return 0;
}
}

// Charge le chemin persistant depuis le registre de l'utilisateur. Une valeur
// absente, vide ou illisible entraîne l'utilisation du chemin par défaut.
std::wstring load_source_directory()
{
    DWORD byteCount = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, REGISTRY_KEY, REGISTRY_VALUE, RRF_RT_REG_SZ,
            nullptr, nullptr, &byteCount) != ERROR_SUCCESS || byteCount <= sizeof(wchar_t))
    {
        return DEFAULT_DIRECTORY;
    }

    std::wstring value(byteCount / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, REGISTRY_KEY, REGISTRY_VALUE, RRF_RT_REG_SZ,
            nullptr, value.data(), &byteCount) != ERROR_SUCCESS)
    {
        return DEFAULT_DIRECTORY;
    }

    while (!value.empty() && value.back() == L'\0')
        value.pop_back();

    return value.empty() ? DEFAULT_DIRECTORY : value;
}

// Écrit le chemin choisi dans une clé HKCU, ce qui ne nécessite pas de droits
// administrateur et conserve une configuration propre à chaque utilisateur.
bool save_source_directory(const std::wstring& directory)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &key, nullptr) != ERROR_SUCCESS)
    {
        return false;
    }

    const auto byteCount = static_cast<DWORD>((directory.size() + 1) * sizeof(wchar_t));
    const LSTATUS status = RegSetValueExW(key, REGISTRY_VALUE, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(directory.c_str()), byteCount);
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

// Ouvre la boîte de dialogue native de sélection d'un dossier. La mémoire de
// l'élément Shell retourné doit être libérée avec CoTaskMemFree.
bool choose_source_directory(HWND owner, const std::wstring& currentDirectory,
    std::wstring& selectedDirectory)
{
    wchar_t displayName[MAX_PATH]{};
    BROWSEINFOW browseInfo{};
    browseInfo.hwndOwner = owner;
    browseInfo.pszDisplayName = displayName;
    browseInfo.lpszTitle = L"Choisissez le dossier contenant les fichiers .ini";
    browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    browseInfo.lpfn = browse_callback;
    browseInfo.lParam = reinterpret_cast<LPARAM>(currentDirectory.c_str());

    PIDLIST_ABSOLUTE item = SHBrowseForFolderW(&browseInfo);
    if (!item)
        return false;

    wchar_t path[MAX_PATH]{};
    const BOOL pathFound = SHGetPathFromIDListW(item, path);
    CoTaskMemFree(item);

    if (!pathFound || path[0] == L'\0')
        return false;

    selectedDirectory = path;
    return true;
}
