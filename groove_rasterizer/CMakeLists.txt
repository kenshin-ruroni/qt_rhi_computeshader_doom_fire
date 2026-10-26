cmake_minimum_required(VERSION 3.16)

project(grooverasterizer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 26)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets GuiPrivate ShaderTools )
find_package(Qt6Quick3D REQUIRED)

qt_standard_project_setup(REQUIRES 6.11)


qt_add_executable(${PROJECT_NAME}
    main.cpp
    rasterizer.h
    rasterizer.cpp
    mainwindow.h mainwindow.cpp
    mainwindow.ui
    rasterize.comp
    sound_system.cpp
    sound_system.h
    dr_libs_impl.cpp
)

target_sources(grooverasterizer
    PRIVATE
        images.qrc
        music.qrc
)



set(COMPUTE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/rasterize.comp")
set(COMPUTE_QSB "${CMAKE_CURRENT_BINARY_DIR}.qsb/rasterize.comp.qsb")

# 2. On force CMake à compiler à la main via qsb
add_custom_command(
    OUTPUT ${COMPUTE_QSB}
    COMMAND Qt6::qsb --glsl "310 es,430" ${COMPUTE_SRC} -o ${COMPUTE_QSB}
    DEPENDS ${COMPUTE_SRC}
    COMMENT "Compilation manuelle du Compute Shader via qsb"
)

set_source_files_properties(${COMPUTE_QSB}
    PROPERTIES QT_RESOURCE_ALIAS "rasterize.comp.qsb"
)

# 3. On l'injecte dans le système de ressources Qt standard (qrc)
qt_add_resources(${PROJECT_NAME} "cshaders"
    PREFIX "/shaders"
    FILES ${COMPUTE_QSB}
)

# Compilation et intégration automatique des shaders
qt_add_shaders(${PROJECT_NAME} "my_shaders"
    PREFIX
        "/shaders"
    FILES
        "render_to_target.vert"
        "render_to_target.frag"
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate
    Qt6::Widgets
    Qt6::Multimedia
)

include_directories( ${CMAKE_SOURCE_DIR} )


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(${PROJECT_NAME} PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appgroove_rasterizer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)



qt_finalize_executable(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
