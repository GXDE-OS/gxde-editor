cmake_minimum_required(VERSION 3.7)

if (NOT DEFINED VERSION)
    set(VERSION 1.2.2)
endif ()

project(gxde-editor)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(QT_MAJOR_VERSION 6)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "sw_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mieee")
endif ()

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

    # generate qm files
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

configure_file(src/environments.h.in environments.h @ONLY)

# Sources files
file(GLOB SRCS "src/*.h" "src/*.cpp"
               "src/thememodule/*.h" "src/thememodule/*.cpp"
               "src/widgets/*.h" "src/widgets/*.cpp")

# Find the library
find_package(PkgConfig REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
find_package(Qt6Network REQUIRED)
find_package(Qt6Widgets REQUIRED)
find_package(Qt6DBus REQUIRED)
find_package(Qt6Concurrent REQUIRED)
find_package(Qt6PrintSupport REQUIRED)
find_package(Qt6Gui REQUIRED)
find_package(Dtk2widget REQUIRED)
find_package(Dtk6Core REQUIRED)
find_package(KF6SyntaxHighlighting REQUIRED)
find_package(KF6Codecs REQUIRED)

# 尝试查找WebEngine模块
find_package(Qt6WebEngine QUIET)
find_package(Qt6WebEngineWidgets QUIET)

include_directories(${Qt6Gui_PRIVATE_INCLUDE_DIRS})

# Tell CMake to create the executable
add_executable(gxde-editor ${SRCS} src/gxde-editor.qrc)
target_include_directories(gxde-editor PUBLIC 
    /usr/include/dtk2/DWidget 
    /usr/include/dtk6/DCore 
    /usr/include/dtk6/DLog 
    /usr/include/dtk6/DGui
    ${OBJECT_BINARY_DIR} 
    ${CMARK_INCLUDE_DIRS}
)
target_link_libraries(gxde-editor
    ${Dtk2widget_LIBRARIES}
    Dtk6::Core
    Qt6::Widgets
    Qt6::DBus
    Qt6::PrintSupport
    Qt6::Concurrent
    Qt6::Network
    Qt6::Core5Compat

    KF6::Codecs
    KF6::SyntaxHighlighting
)

# 判断 Qt6WebEngine 模块是否存在
if(Qt6WebEngine_FOUND AND Qt6WebEngineWidgets_FOUND)
    # 添加模块链接和宏定义
    target_link_libraries(gxde-editor
        Qt6::WebEngine
        Qt6::WebEngineWidgets
    )
    target_compile_definitions(gxde-editor
        PRIVATE
        USE_WEBENGINE
    )
endif()

set(CMAKE_INSTALL_PREFIX /usr)

add_subdirectory("gxedit")
add_subdirectory("dedit")

include(CTest)

if (BUILD_TESTING)
    find_package(Qt6Test REQUIRED)

    add_executable(gxde-editor-tests
        tests/syntaxutils_test.cpp
        src/syntaxutils.cpp
    )
    target_link_libraries(gxde-editor-tests
        Qt6::Core
        Qt6::Test
        KF6::SyntaxHighlighting
    )
    add_test(NAME gxde-editor-tests COMMAND gxde-editor-tests)
endif ()

# Install qm files
file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES}
        DESTINATION share/gxde-editor/translations)

install(TARGETS gxde-editor DESTINATION bin)
install(FILES gxde-editor.desktop DESTINATION share/applications)
install(FILES gxde-editor.svg DESTINATION share/icons/)

install(DIRECTORY src/themes
        DESTINATION share/gxde-editor)
