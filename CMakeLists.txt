cmake_minimum_required(VERSION 3.24)
project(enchantment_tweaks CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(BUILD_SHARED_LIBS OFF)

option(ENCHANTMENT_TWEAKS_ENABLE_COVERAGE "Instrument project code for coverage" OFF)
if(ENCHANTMENT_TWEAKS_ENABLE_COVERAGE)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "Coverage instrumentation currently requires GCC.")
  endif()
endif()

if(MSVC)
  add_compile_options(/W4 /Zi)
else()
  add_compile_options(-Wall -Wno-unused-function -Wextra -g)
endif()

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS "src/core/*.cpp")

# Core library
add_library(enchantment_tweaks_core
    ${CORE_SOURCES} ${CMAKE_SOURCE_DIR}/src/libraries/stb_image_impl.cpp)
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
if(ENCHANTMENT_TWEAKS_ENABLE_COVERAGE)
  target_compile_options(enchantment_tweaks_core PRIVATE --coverage)
endif()

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

  find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3-shared)

  find_package(Driveker 0.2.0 CONFIG QUIET)
  if(NOT TARGET Driveker::Core)
    set(DRIVEKER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DRIVEKER_BUILD_GL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
      driveker
      GIT_REPOSITORY https://github.com/projectAccounth/driveker.git
      GIT_TAG v0.2.1
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(driveker)
  elseif(NOT TARGET Driveker::GL)
    message(STATUS "Driveker GL module not found. Building it from source.")
    find_package(OpenGL REQUIRED)
    FetchContent_Declare(
      driveker_gl_source
      GIT_REPOSITORY https://github.com/projectAccounth/driveker.git
      GIT_TAG v0.2.0
      GIT_SHALLOW TRUE
      SOURCE_SUBDIR src/gl
    )
    FetchContent_MakeAvailable(driveker_gl_source)

    file(GLOB DRIVEKER_GL_SOURCES CONFIGURE_DEPENDS
      "${driveker_gl_source_SOURCE_DIR}/src/gl/*.cpp")
    add_library(et_driveker_gl STATIC ${DRIVEKER_GL_SOURCES})
    add_library(Driveker::GL ALIAS et_driveker_gl)
    target_compile_features(et_driveker_gl PUBLIC cxx_std_20)
    target_include_directories(et_driveker_gl
      PUBLIC "${driveker_gl_source_SOURCE_DIR}/include")
    target_link_libraries(et_driveker_gl
      PUBLIC Driveker::Core
      PRIVATE SDL3::SDL3-shared OpenGL::GL)
  endif()

  if(NOT TARGET Driveker::GL)
    message(FATAL_ERROR "Driveker was configured without its GL module.")
  endif()

  file(GLOB_RECURSE SEL_SRC_FILES CONFIGURE_DEPENDS "apps/native/app/*.cpp")

  add_library(et_native STATIC ${SEL_SRC_FILES})
  target_include_directories(et_native
        PUBLIC ${CMAKE_SOURCE_DIR}/apps/native
        PRIVATE ${CMAKE_SOURCE_DIR}/include
    )

  set(NFD_INSTALL OFF CACHE BOOL "" FORCE)
  set(NFD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  # Wayland only tags the dialog's parent window, the gtk dialog itself works without it
  set(NFD_WAYLAND OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
        nfd
        GIT_REPOSITORY https://github.com/btzy/nativefiledialog-extended.git
        GIT_TAG v1.4.1
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""   # tests
    )
  FetchContent_MakeAvailable(nfd)
  target_link_libraries(et_native PRIVATE nfd enchantment_tweaks_core curl)

  add_executable(enchantment_tweaks_native apps/native/main.cpp)
  target_include_directories(enchantment_tweaks_native PRIVATE ${CMAKE_SOURCE_DIR}/apps/native)
  target_link_libraries(enchantment_tweaks_native PRIVATE enchantment_tweaks_core et_native curl)
  if(ENCHANTMENT_TWEAKS_ENABLE_COVERAGE)
    target_link_options(enchantment_tweaks_native PRIVATE --coverage)
  endif()

  # windows pops a console window if one is not a Win32 graphical executable
  if(WIN32)
    set_target_properties(enchantment_tweaks_native PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()

  set_target_properties(enchantment_tweaks_native PROPERTIES
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
  target_sources(et_native PRIVATE
        "${rmlui_SOURCE_DIR}/Backends/RmlUi_Platform_SDL.cpp"
    )
  target_include_directories(et_native PRIVATE "${rmlui_SOURCE_DIR}/Backends")
  target_compile_definitions(et_native PRIVATE RMLUI_SDL_VERSION_MAJOR=3)
  target_link_libraries(et_native PUBLIC RmlUi::Core Driveker::GL SDL3::SDL3-shared)

  add_executable(ui_snapshot apps/native/tools/ui_snapshot.cpp apps/native/tools/soft_renderer.cpp)
  target_include_directories(ui_snapshot PRIVATE ${CMAKE_SOURCE_DIR}/apps/native ${CMAKE_SOURCE_DIR}/include/libraries)
  target_link_libraries(ui_snapshot PRIVATE et_native enchantment_tweaks_core RmlUi::Core)
  if(ENCHANTMENT_TWEAKS_ENABLE_COVERAGE)
    target_link_options(ui_snapshot PRIVATE --coverage)
  endif()

  add_custom_command(TARGET enchantment_tweaks_native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/apps/native/app/assets"
            "$<TARGET_FILE_DIR:enchantment_tweaks_native>/assets"
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
