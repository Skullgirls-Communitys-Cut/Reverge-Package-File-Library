cmake_minimum_required(VERSION 3.20)
project(RPFL LANGUAGES CXX)

# Настройки C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции проекта
option(BUILD_SHARED_LIBS "Build shared library instead of static" OFF)
option(BUILD_TESTS "Build tests" OFF)  # По умолчанию OFF, если нет тестов

# Список исходных файлов
set(RPFL_SOURCES
    src/archive_reader.cpp
    src/archive_file.cpp
    src/memory_mapped_file.cpp
    src/archive_writer.cpp
)

# Создаем библиотеку
add_library(rpfl ${RPFL_SOURCES})

# Указываем заголовочные файлы
target_include_directories(rpfl
    PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Для Windows: специальные настройки
if(WIN32)
    target_compile_definitions(rpfl PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(rpfl PRIVATE /W4 /WX)
endif()

# Для Linux: добавляем системные библиотеки
if(UNIX AND NOT APPLE)
    target_link_libraries(rpfl PUBLIC pthread)
    # Для mmap, madvise
    target_link_libraries(rpfl PUBLIC ${CMAKE_DL_LIBS})
    target_compile_options(rpfl PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Настройки для разных конфигураций
set_target_properties(rpfl PROPERTIES
    DEBUG_POSTFIX "d"
    RELEASE_POSTFIX ""
    VERSION 1.0.0
    SOVERSION 1
)

# Установка (опционально)
install(TARGETS rpfl
    EXPORT rpfl-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

# Тесты - только если есть папка tests и BUILD_TESTS=ON
if(BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    
    # Ищем файлы тестов
    file(GLOB TEST_SOURCES "tests/*.cpp")
    if(TEST_SOURCES)
        add_executable(test_rpfl ${TEST_SOURCES})
        target_link_libraries(test_rpfl PRIVATE rpfl)
        add_test(NAME test_rpfl COMMAND test_rpfl)
    endif()
endif()