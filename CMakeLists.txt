cmake_minimum_required(VERSION 3.21)

project(AudioToneStudio VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Widgets
    Multimedia
)

qt_add_executable(AudioToneStudio
    main.cpp
)

target_link_libraries(AudioToneStudio PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
)

set_target_properties(AudioToneStudio PROPERTIES
    WIN32_EXECUTABLE TRUE
)
