#pragma once

#include <string>
#include <vector>

// Assemble proprement un dossier et un nom de fichier, y compris lorsque le
// dossier se termine déjà par un séparateur.
std::wstring build_path(const std::wstring& directory, const std::wstring& fileName);

// Retourne les fichiers .ini du dossier. Le fichier réservé cnx.ini est toujours
// exclu, quelle que soit la casse de son nom.
std::vector<std::wstring> list_ini_files(const std::wstring& directory);

// Copie le fichier sélectionné vers cnx.ini en passant par cnx.tmp afin de ne
// jamais écrire directement dans le fichier de connexion actif.
bool activate_ini_file(const std::wstring& directory, const std::wstring& fileName);
