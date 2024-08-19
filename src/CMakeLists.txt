cmake_minimum_required(VERSION 3.2)

project(coroutines-cpp-mt C CXX)

file(GLOB coroutines_cpp_mt_SRC 
	CONFIGURE_DEPENDS 
    "*.h"
	"*.hpp"
    "*.cpp"
)


add_library(coroutines_cpp_mt SHARED STATIC ${coroutines_cpp_mt_SRC})
#set_property(TARGET coroutines_cpp_mt PROPERTY
#  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS coroutines_cpp_mt DESTINATION ${COROUTINES_CPP_MT_INSTALL_LIB_DIR})
#install(FILES coroutines_cpp_mt.h DESTINATION ${COROUTINES_CPP_MT_INSTALL_INCLUDE_DIR})
