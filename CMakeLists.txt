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


include_directories(third/spdlog/include)

find_package(PkgConfig REQUIRED)
pkg_check_modules(uchardet REQUIRED IMPORTED_TARGET uchardet)
message(STATUS "STATIC_LIBRARIES: ${uchardet_STATIC_LIBRARIES}")
message(STATUS "STATIC_LDFLAGS: ${uchardet_STATIC_LDFLAGS}")
message(STATUS "STATIC_LDFLAGS_OTHER: ${uchardet_STATIC_LDFLAGS_OTHER}")

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

target_link_libraries(TXTReader
    PRIVATE
    Qt6::Widgets
    Qt6::Core
    ${Boost_LIBRARIES}
    ${uchardet_STATIC_LIBRARIES}
)
if(WIN32)
    set_target_properties(TXTReader PROPERTIES WIN32_EXECUTABLE ON)
endif()

install(TARGETS TXTReader DESTINATION bin)
