add_executable(nl_tests
  test_main.cpp
  test_cheats.cpp
  test_format.cpp
  test_scroll_anim.cpp
  test_store_io.cpp
  ${CMAKE_SOURCE_DIR}/src/Cheats.cpp
  ${CMAKE_SOURCE_DIR}/src/Embedded.cpp
  ${CMAKE_SOURCE_DIR}/src/Log.cpp
  ${CMAKE_SOURCE_DIR}/src/Paths.cpp
  ${CMAKE_SOURCE_DIR}/src/ui/Scroll.cpp
)

target_compile_features(nl_tests PRIVATE cxx_std_20)
target_include_directories(nl_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(nl_tests SYSTEM PRIVATE ${NL_VENDOR})
target_compile_definitions(nl_tests PRIVATE
  NL_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
)

if(MSVC)
  target_compile_options(nl_tests PRIVATE /permissive- /W4 /WX /utf-8 /MP)
  target_compile_definitions(nl_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
else()
  target_compile_options(nl_tests PRIVATE -Wall -Wextra -Werror)
endif()

add_test(NAME nl_tests COMMAND nl_tests)
