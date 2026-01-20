cmake_minimum_required(VERSION 4.0)

project(Reverge_Package_File_Library VERSION 0.1.0)

# Настройка стандарта C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RPFL_BUILD_STATIC "Build the library statically" ON)
option(RPFL_BUILD_TEST "Build the library tests" OFF)

set(SOURCES
    src/archive_file.cpp
    src/archive_reader.cpp
    src/archive_writer.cpp
    src/memory_mapped_file.cpp
)

set(HEADERS
    include/archive_common.hpp
    include/archive_exception.hpp
    include/archive_file.hpp
    include/archive_reader.hpp
    include/archive_writer.hpp
    include/memory_mapped_file.hpp
    include/RPFL.h
)

if(RPFL_BUILD_STATIC) 
	add_library(RPFL STATIC ${HEADERS} ${SOURCES})
else()
	add_library(RPFL SHARED ${HEADERS} ${SOURCES})
endif()

target_include_directories(RPFL
    PUBLIC
	include
)

if(RPFL_BUILD_TEST) 
	add_executable(Test test/test.cpp)
	target_link_libraries(Test PRIVATE RPFL)
endif()