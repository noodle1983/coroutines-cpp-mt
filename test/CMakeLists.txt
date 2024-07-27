cmake_minimum_required(VERSION 3.2)

project(coroutines_cpp_mt_tests)

add_subdirectory(lib/googletest)

include_directories(${COROUTINES_CPP_MT_HEADERS_DIR})
include_directories(lib/googletest/googletest/include)

set(SOURCE_FILES main.cpp src/coroutines_cpp_mt_tests.cpp)

add_executable(coroutines_cpp_mt_tests ${SOURCE_FILES})
target_link_libraries(coroutines_cpp_mt_tests coroutines_cpp_mt gtest)
#set_property(TARGET coroutines_cpp_mt_tests PROPERTY
#  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS coroutines_cpp_mt_tests DESTINATION bin)

