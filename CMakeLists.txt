cmake_minimum_required(VERSION 3.16)
project(BlogClient LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Network
        Sql
)
qt_standard_project_setup()

# 添加包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${CMAKE_CURRENT_SOURCE_DIR}/src/models
    ${CMAKE_CURRENT_SOURCE_DIR}/src/database
)

# 设置源文件
set(PROJECT_SOURCES
    main.cpp
    BlogClient.ui
    BlogClient.h
    BlogClient.cpp
    resources.qrc
    src/api/WordPressAPI.h
    src/api/WordPressAPI.cpp
    src/models/Post.h
    src/models/Post.cpp
    src/models/Category.h
    src/models/Category.cpp
    src/models/Tag.h
    src/models/Tag.cpp
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Network
        Qt::Sql
)

