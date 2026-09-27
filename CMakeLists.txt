cmake_minimum_required(VERSION 3.21)
project(WPaint VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Svg)
qt_standard_project_setup()

qt_add_executable(wpaint
    src/main.cpp
    resources.qrc
    src/Theme.cpp
    src/DrawingUtils.cpp
    src/FluentCombo.cpp
    src/FluentSlider.cpp
    src/FluentSwitch.cpp
    src/ColorState.cpp
    src/ColorDialog.cpp
    src/LayerStack.cpp
    src/PaletteEditor.cpp
    src/PalettePresetCombo.cpp
    src/PalettePresets.cpp
    src/LayersPanel.cpp
    src/Commands.cpp
    src/Tool.cpp
    src/CanvasView.cpp
    src/SizeSliderPanel.cpp
    src/CopilotPanel.cpp
    src/ResizeDialog.cpp
    src/Settings.cpp
    src/SettingsDialog.cpp
    src/SettingsRow.cpp
    src/ShortcutEdit.cpp
    src/Shortcuts.cpp
    src/SettingsPage.cpp
    src/SettingsSwatchButton.cpp
    src/MainWindow.cpp
)

target_include_directories(wpaint PRIVATE src)

target_link_libraries(wpaint PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg)

target_compile_definitions(wpaint PRIVATE
    WP_VERSION="${PROJECT_VERSION}"
    WP_APP_NAME="${PROJECT_NAME}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wpaint PRIVATE -Wall -Wextra)
endif()

install(TARGETS wpaint RUNTIME DESTINATION bin)