#pragma once

#include <windows.h>
#include <string>

// Lit le dossier source enregistré pour l'utilisateur courant. Si aucun
// réglage n'existe encore, retourne C:\iSuiteExpert.
std::wstring load_source_directory();

// Enregistre le dossier source dans HKCU afin de le retrouver aux prochains
// lancements. Retourne false si Windows refuse l'écriture.
bool save_source_directory(const std::wstring& directory);

// Affiche le sélecteur de dossier Windows en partant du dossier actuel.
// Retourne true et remplit selectedDirectory uniquement si l'utilisateur valide.
bool choose_source_directory(HWND owner, const std::wstring& currentDirectory,
    std::wstring& selectedDirectory);
