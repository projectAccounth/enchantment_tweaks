# Tests CMakeLists.txt

# Everything the tests need now lives in enchantment_tweaks_core (including stb).
# Kept as an interface target so existing test targets can keep linking it.
add_library(test_core INTERFACE)
target_include_directories(test_core INTERFACE ${CMAKE_SOURCE_DIR}/include)

enable_testing()

# Dynamically discover all test executables
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*_tests.cpp"
)

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_include_directories(${TEST_NAME} PRIVATE 
        ${CMAKE_SOURCE_DIR}/include 
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    target_link_libraries(${TEST_NAME} PRIVATE enchantment_tweaks_core test_core)
    
    # Most tests use fixtures relative to the tests directory
    target_compile_definitions(${TEST_NAME} PRIVATE
        TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endforeach()

set_tests_properties(
    resource_pack_custom_texture_tests
    resource_pack_load_tests
    resource_pack_roundtrip_tests
    PROPERTIES SKIP_RETURN_CODE 77
)
