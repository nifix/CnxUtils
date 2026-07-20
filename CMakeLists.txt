cmake_minimum_required(VERSION 4.0)
project(CnxUtils)

set(CMAKE_CXX_STANDARD 20)
add_executable(CnxUtils WIN32
    main.cpp
    directory_config.cpp
    directory_config.h
    ini_files.cpp
    ini_files.h
    App.rc
)

target_link_libraries(CnxUtils PRIVATE shlwapi shell32 ole32 advapi32)
