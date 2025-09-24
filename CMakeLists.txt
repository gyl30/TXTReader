cmake_minimum_required(VERSION 3.16)
project(TXTreader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_DEBUG_LIBS ON)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.83.0 REQUIRED COMPONENTS regex locale)
include_directories(${Boost_INCLUDE_DIRS})
message(STATUS "Found Boost:    V${Boost_VERSION} (in ${Boost_DIR})")

find_package(uchardet REQUIRED)
message(STATUS "Found uchardet: V${uchardet_VERSION} (in ${uchardet_DIR})")
get_filename_component(UCHARDET_CONFIG_PATH "${uchardet_DIR}" REALPATH)
get_filename_component(UCHARDET_INSTALL_PREFIX "${UCHARDET_CONFIG_PATH}" DIRECTORY)
get_filename_component(UCHARDET_INSTALL_PREFIX "${UCHARDET_INSTALL_PREFIX}" DIRECTORY)
get_filename_component(UCHARDET_INSTALL_PREFIX "${UCHARDET_INSTALL_PREFIX}" DIRECTORY)
set(UCHARDET_INCLUDE_DIR "${UCHARDET_INSTALL_PREFIX}/include")
message(STATUS "Uchardet include path: ${UCHARDET_INCLUDE_DIR}")

add_executable(TXTReader
    main.cpp
    log.cpp
    splitter.cpp
    main_window.h
    main_window.cpp
    novel_manager.h
    novel_manager.cpp
    novel_view.cpp
)

target_include_directories(TXTReader
    PRIVATE
    ${Boost_INCLUDE_DIRS}
    third/spdlog/include
    ${UCHARDET_INCLUDE_DIR}
)

target_link_libraries(TXTReader
    PRIVATE
    Qt6::Widgets
    Qt6::Core
    ${Boost_LIBRARIES}
    uchardet::libuchardet
)

if(WIN32)
    set_target_properties(TXTReader PROPERTIES WIN32_EXECUTABLE ON)
endif()

install(TARGETS TXTReader DESTINATION bin)
