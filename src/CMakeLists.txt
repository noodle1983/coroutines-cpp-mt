cmake_minimum_required(VERSION 3.2)

project(coroutines-cpp-mt C CXX)

file(GLOB_RECURSE coroutines_cpp_mt_SRC 
	CONFIGURE_DEPENDS 
    "*.h"
	"*.hpp"
    "*.cpp"
)

function(assign_file_group)
    foreach(_source IN ITEMS ${ARGN})
        if (IS_ABSOLUTE "${_source}")
            file(RELATIVE_PATH _source_rel "${CMAKE_CURRENT_SOURCE_DIR}" "${_source}")
        else()
            set(_source_rel "${_source}")
        endif()
        get_filename_component(_source_path "${_source_rel}" PATH)
        string(REPLACE "/" "\\" _source_path_msvc "${_source_path}")
        source_group("${_source_path_msvc}" FILES "${_source}")
    endforeach()
endfunction(assign_file_group)
 
function(assign_files_group)
	foreach(_source IN ITEMS ${ARGN})
		assign_file_group(${_source})
	endforeach()
endfunction(assign_files_group)

assign_files_group(${coroutines_cpp_mt_SRC})

add_library(coroutines_cpp_mt SHARED STATIC ${coroutines_cpp_mt_SRC})
#set_property(TARGET coroutines_cpp_mt PROPERTY
#  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

install(TARGETS coroutines_cpp_mt DESTINATION ${COROUTINES_CPP_MT_INSTALL_LIB_DIR})
#install(FILES coroutines_cpp_mt.h DESTINATION ${COROUTINES_CPP_MT_INSTALL_INCLUDE_DIR})
