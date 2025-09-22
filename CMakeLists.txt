cmake_minimum_required(VERSION 3.16)
project(TXTreader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

set(Boost_USE_DEBUG_LIBS ON)
set(Boost_USE_RELEASE_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.83.0 REQUIRED COMPONENTS regex)

include_directories(${Boost_INCLUDE_DIRS})

add_executable(TXTReader
    main.cpp
    splitter.cpp
    main_window.h
    main_window.cpp
    novel_manager.h
    novel_manager.cpp
)

target_link_libraries(TXTReader
    PRIVATE
    Qt6::Widgets
    Qt6::Core
    ${Boost_LIBRARIES}
)
if(WIN32)
    set_target_properties(TXTReader PROPERTIES WIN32_EXECUTABLE ON)
endif()

install(TARGETS TXTReader DESTINATION bin)
