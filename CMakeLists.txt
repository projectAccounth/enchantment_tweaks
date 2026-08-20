cmake_minimum_required(VERSION 3.23)
project(enchantment_tweaks CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /Zi)
else()
    add_compile_options(-Wall -Wextra -g)
endif()

file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS "src/*.cpp")

# native
if(NOT EMSCRIPTEN)
    include(FetchContent)

    find_package(CURL QUIET)

    if(NOT CURL_FOUND)
        message(STATUS "libcurl not found on system. Building.")

        set(BUILD_CURL_EXE OFF CACHE INTERNAL "library")
        set(BUILD_TESTING OFF CACHE INTERNAL "")
        set(CURL_DISABLE_TESTS ON CACHE INTERNAL "")
        set(CURL_DISABLE_DOCS ON CACHE INTERNAL "")
        
        set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
        set(BUILD_STATIC_LIBS ON CACHE INTERNAL "")

        set(CURL_USE_OPENSSL OFF CACHE INTERNAL "")
        set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")

        FetchContent_Declare(
            curl
            GIT_REPOSITORY https://github.com/curl/curl.git
            GIT_TAG        curl-8_21_0
        )

        FetchContent_MakeAvailable(curl)
    endif()

    add_executable(native ${SHARED_SOURCES} apps/native/main.cpp)
    target_include_directories(native PRIVATE include)
    target_include_directories(native PRIVATE apps/native)

    target_link_libraries(native PRIVATE libcurl)
    
    set_target_properties(native PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# With Emscripten for WASM targets
if(EMSCRIPTEN)
    add_executable(wasm_api ${SHARED_SOURCES} apps/wasm/main.cpp)
    target_include_directories(wasm_api PRIVATE include)
    target_include_directories(wasm_api PRIVATE apps/wasm)

    target_link_options(wasm_api PRIVATE 
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='createWasmModule'"
        "-sALLOW_MEMORY_GROWTH=1" 
    )

    set_target_properties(wasm_api PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()