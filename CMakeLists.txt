cmake_minimum_required (VERSION 3.7)
project (eml VERSION 1.1)
include(FindPkgConfig)
include(CheckIncludeFiles) # AC_CHECK_INCLUDE equivalent (verifies if file.h exists)
include(CheckFunctionExists) # Checks existence of a function in a lib
include(CMakeDependentOption)
include(GNUInstallDirs) # gnu lib tree

macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

set(CMAKE_MODULE_PATH
        ${CMAKE_MODULE_PATH}
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules
        )

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c99)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/config")
include_directories("${PROJECT_SOURCE_DIR}/src")

include_directories("${PROJECT_SOURCE_DIR}/include")

foreach(LIB ${COMMA_SEPARATED_LIST})
    set(PRIVATE_LIBS "${PRIVATE_LIBS} -l${LIB}")
endforeach()

configure_file(config/eml.pc.in eml.pc @ONLY)

pkg_search_module(CONFUSE REQUIRED libconfuse)
check_include_files(nvml.h HAVE_NVML)
check_include_files(miclib.h HAVE_MIC)
check_include_files(jni.h HAVE_JNI)

find_package(Doxygen)

option(BUILD_DOCUMENTATION "Create and install the HTML based API
documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCUMENTATION)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(doxyfile_in config/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/config/Doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    message("Doxygen build started.")

    add_custom_target(doc
            COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM)

endif()


add_subdirectory(src)
add_subdirectory(utils)

#print_all_variables()