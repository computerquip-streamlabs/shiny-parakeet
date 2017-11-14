cmake_minimum_required(VERSION 3.0.0)

set(CMAKE_PROJECT_NAME "asio_named_pipes")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0600)
endif()

# Define header-based ASIO interface library
add_library(asio INTERFACE)

target_include_directories(asio INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/asio/asio/include
)

target_compile_definitions(asio INTERFACE
    ASIO_STANDALONE
)

add_subdirectory(src)