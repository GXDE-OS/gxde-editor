cmake_minimum_required(VERSION 3.7)

if (NOT DEFINED VERSION)
    set(VERSION 1.2.2)
endif ()

project(gxde-editor)

#set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 11)
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
find_package(Qt5Network REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Concurrent REQUIRED)
find_package(Qt5PrintSupport REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(DtkWidget REQUIRED)
find_package(DtkCore REQUIRED)
find_package(KF5SyntaxHighlighting)
find_package(KF5Codecs)

include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

# Tell CMake to create the executable
add_executable(gxde-editor ${SRCS} src/gxde-editor.qrc)
target_include_directories(gxde-editor PUBLIC ${DtkWidget_INCLUDE_DIRS} ${OBJECT_BINARY_DIR} ${CMARK_INCLUDE_DIRS})
target_link_libraries(gxde-editor
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5PrintSupport_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5Network_LIBRARIES}

    KF5::Codecs
    KF5::SyntaxHighlighting
)


set(CMAKE_INSTALL_PREFIX /usr)

add_subdirectory("gxedit")
add_subdirectory("dedit")

# Install qm files
file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES}
        DESTINATION share/gxde-editor/translations)

install(TARGETS gxde-editor DESTINATION bin)
install(FILES gxde-editor.desktop DESTINATION share/applications)
install(FILES gxde-editor.svg DESTINATION share/icons/)

install(DIRECTORY src/themes
        DESTINATION share/gxde-editor)
