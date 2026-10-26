cmake_minimum_required(VERSION 3.24)
project(enchantment_tweaks CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

if(MSVC)
    add_compile_options(/W4 /Zi)
else()
    add_compile_options(-Wall -Wno-unused-function -Wextra -g)
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")

# Core library
add_library(enchantment_tweaks_core ${CORE_SOURCES})
target_include_directories(enchantment_tweaks_core PUBLIC include)

# Compile miniz for ZIP handling (used by both native and WASM)
add_library(miniz STATIC
    src/libraries/miniz/miniz.c
    src/libraries/miniz/miniz_tdef.c
    src/libraries/miniz/miniz_tinfl.c
    src/libraries/miniz/miniz_zip.c
)
target_include_directories(miniz PUBLIC include)
target_include_directories(miniz PRIVATE include/libraries/miniz)
target_compile_options(miniz PRIVATE
    -DMINIZ_NO_TIME
    -DMINIZ_NO_ARCHIVE_WRITING_APIS
    -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES
    -DMINIZ_NO_STDIO
)
set_target_properties(miniz PROPERTIES LINKER_LANGUAGE C)

target_link_libraries(enchantment_tweaks_core PUBLIC miniz)

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

        set(CURL_USE_OPENSSL ON CACHE INTERNAL "")
        set(CURL_USE_LIBPSL OFF CACHE INTERNAL "")

        FetchContent_Declare(
            curl
            GIT_REPOSITORY https://github.com/curl/curl.git
            GIT_TAG        curl-8_21_0
        )

        FetchContent_MakeAvailable(curl)
    endif()

    find_package(Driveker 0.1 CONFIG REQUIRED)
    find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-shared)

    file(GLOB_RECURSE SEL_SRC_FILES CONFIGURE_DEPENDS "apps/native/selector/*.cpp")

    add_library(enchantment_tweaks_selector STATIC ${SEL_SRC_FILES})
    target_include_directories(enchantment_tweaks_selector
        PUBLIC ${CMAKE_SOURCE_DIR}/apps/native
        PRIVATE ${CMAKE_SOURCE_DIR}/include
    )

    add_executable(native apps/native/main.cpp)
    target_include_directories(native PRIVATE ${CMAKE_SOURCE_DIR}/apps/native)
    target_link_libraries(native PRIVATE enchantment_tweaks_core enchantment_tweaks_selector curl)

    # windows pops a console window if one is not a Win32 graphical executable
    if(WIN32)
        set_target_properties(native PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()

    set_target_properties(native PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    FetchContent_Declare(
        rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG        6.3
    )
    set(RMLUI_SAMPLES OFF CACHE INTERNAL "")
    set(RMLUI_BUILD_TESTS OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(rmlui)
    target_sources(enchantment_tweaks_selector PRIVATE
        "${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp"
    )
    target_include_directories(enchantment_tweaks_selector PRIVATE "${rmlui_SOURCE_DIR}/Backends")
    target_compile_definitions(enchantment_tweaks_selector PRIVATE RMLUI_SDL_VERSION_MAJOR=3)
    target_link_libraries(enchantment_tweaks_selector PUBLIC RmlUi::Core Driveker::GL SDL3::SDL3-shared)

    # Make the selector usable from the default native binary working directory.
    add_custom_command(TARGET native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/apps/native/selector/assets"
            "$<TARGET_FILE_DIR:native>/selector-assets"
    )
endif()

# With Emscripten for WASM targets
if(EMSCRIPTEN)

    add_executable(wasm_api apps/wasm/main.cpp)
    target_link_libraries(wasm_api PRIVATE enchantment_tweaks_core)

    target_link_options(wasm_api PRIVATE
        "--bind"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME='createWasmModule'"
        "-sALLOW_MEMORY_GROWTH=1"
        "-fexceptions"
    )

    set_target_properties(wasm_api PROPERTIES
        SUFFIX ".js"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    # Test target for WASM API
    enable_testing()
    add_custom_target(wasm_api_test
        COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/apps/wasm/test_wasm_api.js
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        DEPENDS wasm_api
        COMMENT "Running WASM API tests"
    )
    add_test(NAME wasm_api_test COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/apps/wasm/test_wasm_api.js
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(wasm_api_test PROPERTIES
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()

# Tests
option(ENCHANTMENT_TWEAKS_BUILD_TESTS "Build tests" ON)
if(ENCHANTMENT_TWEAKS_BUILD_TESTS)
    add_subdirectory(tests)
endif()
