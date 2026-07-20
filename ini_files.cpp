#include "ini_files.h"

#include <windows.h>
#include <Shlwapi.h>
#include <fstream>

// Construit un chemin sans ajouter un second antislash après un dossier racine
// tel que C:\.
std::wstring build_path(const std::wstring& directory, const std::wstring& fileName)
{
    if (!directory.empty() && (directory.back() == L'\\' || directory.back() == L'/'))
        return directory + fileName;

    return directory + L"\\" + fileName;
}

// Parcourt tous les fichiers .ini du dossier avec l'API Windows. cnx.ini est le
// fichier cible du programme : il est donc explicitement et toujours ignoré.
std::vector<std::wstring> list_ini_files(const std::wstring& directory)
{
    std::vector<std::wstring> files;
    const std::wstring pattern = build_path(directory, L"*.ini");

    WIN32_FIND_DATAW fileData{};
    HANDLE search = FindFirstFileW(pattern.c_str(), &fileData);
    if (search == INVALID_HANDLE_VALUE)
        return files;

    do
    {
        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        // La comparaison ignore la casse : CNX.INI et Cnx.ini sont aussi exclus.
        if (_wcsicmp(fileData.cFileName, L"cnx.ini") == 0)
            continue;

        files.emplace_back(fileData.cFileName);
    } while (FindNextFileW(search, &fileData));

    FindClose(search);
    return files;
}

// Prépare entièrement cnx.tmp, puis remplace cnx.ini en une seule opération.
// Les solutions de repli couvrent notamment les systèmes de fichiers sur
// lesquels ReplaceFileW n'est pas disponible.
bool activate_ini_file(const std::wstring& directory, const std::wstring& fileName)
{
    if (!PathFileExistsW(directory.c_str()))
        return false;

    const std::wstring source = build_path(directory, fileName);
    const std::wstring destination = build_path(directory, L"cnx.ini");
    const std::wstring temporary = build_path(directory, L"cnx.tmp");

    const std::ifstream input(source.c_str(), std::ios::binary);
    if (!input)
        return false;

    {
        std::ofstream output(temporary.c_str(), std::ios::binary | std::ios::trunc);
        if (!output)
            return false;

        output << input.rdbuf();
        output.flush();
        if (!output)
        {
            output.close();
            DeleteFileW(temporary.c_str());
            return false;
        }
    }

    if (PathFileExistsW(destination.c_str()))
    {
        if (!ReplaceFileW(destination.c_str(), temporary.c_str(), nullptr,
                REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)
            && !MoveFileExW(temporary.c_str(), destination.c_str(),
                MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            DeleteFileW(temporary.c_str());
            return false;
        }
    }
    else if (!MoveFileExW(temporary.c_str(), destination.c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
    {
        if (!CopyFileW(temporary.c_str(), destination.c_str(), FALSE))
        {
            DeleteFileW(temporary.c_str());
            return false;
        }

        DeleteFileW(temporary.c_str());
    }

    return true;
}
