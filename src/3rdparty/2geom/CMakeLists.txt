cmake_minimum_required(VERSION 3.12)
cmake_policy(SET CMP0054 NEW)


set(2GEOM_MAJOR_VERSION 1)
set(2GEOM_MINOR_VERSION 3)
set(2GEOM_PATCH_VERSION 0)
set(2GEOM_VERSION ${2GEOM_MAJOR_VERSION}.${2GEOM_MINOR_VERSION}.${2GEOM_PATCH_VERSION} CACHE INTERNAL "" FORCE)
set(2GEOM_ABI_VERSION ${2GEOM_MAJOR_VERSION}.${2GEOM_MINOR_VERSION}.0)

project(lib2geom
    VERSION
        ${2GEOM_VERSION}
    LANGUAGES
        CXX
        C     # C is required by CHECK_SYMBOL_EXISTS
    )

set(2GEOM_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/include" CACHE INTERNAL "")
include_directories("${CMAKE_CURRENT_LIST_DIR}/src/2geom") # for private headers/template support.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # enforces -std=c++17 instead of -std=gnu++17
set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_SKIP_RPATH:BOOL OFF)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts)
set(THREADS_PREFER_PTHREAD_FLAG ON)

include(GNUInstallDirs)
include(CheckCXXSourceCompiles)

# Find dependencies
find_package(Boost 1.60 REQUIRED)
find_package(DoubleConversion REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Cython)
find_package(Threads)
pkg_check_modules(GTK3 gtk+-3.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(CAIRO cairo)
pkg_check_modules(GSL REQUIRED gsl)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(2GEOM_STANDALONE TRUE)
endif()

option(2GEOM_TESTING "Build tests" "${2GEOM_STANDALONE}")

if (2GEOM_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()


check_cxx_source_compiles("#include <math.h>\nint main() { double a=0.5,b=0.5,c=0.5; sincos(a, &b, &c); return 0; }" HAVE_SINCOS)
if(HAVE_SINCOS)
    add_definitions(-DHAVE_SINCOS)
endif()
if(GSL_FOUND)
    add_definitions(-DHAVE_GSL)
endif()
if(CAIRO_FOUND)
    add_definitions(-DHAVE_CAIRO)
endif()
if(PYCAIRO_FOUND)
    add_definitions(-DHAVE_PYCAIRO)
endif()


#    SET(NEEDS_GSL
#        differential-constraint
#        root-finder-comparer
#        # contour
#        sb-to-bez
#    )

add_compile_options(-Wall -Wformat-security -Woverloaded-virtual -Wpointer-arith -Werror=return-type)
add_compile_options(-O2)

# suppress the very annoying "#pragma ms_struct" Clang warning, caused by -mms-bitfield required for GTK
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    if(WIN32)
        add_compile_options(-Wno-ignored-attributes)
    endif()
    add_compile_options(-Wno-unused-local-typedef)
endif()

option(WITH_PROFILING
    "Build lib2geom with profiling enabled"
    OFF)

if(WITH_PROFILING)
    add_compile_options(-pg)
    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -pg")
endif()

option(2GEOM_BUILD_SHARED
    "Build lib2geom and libtoy as shared libraries."
    OFF)

if(2GEOM_BUILD_SHARED)
    set(LIB_TYPE SHARED)
else()
    set(LIB_TYPE STATIC)
endif()

option(2GEOM_USE_GPL_CODE
    "Build lib2geom with GPL licensed Code."
    ON)

if(2GEOM_USE_GPL_CODE)
    add_definitions(-DGPL_TAINT)
endif()

if(2GEOM_STANDALONE)
    include(CMakeScripts/Dist.cmake)
endif()

# make unistall target
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

add_custom_target(uninstall_${PROJECT_NAME}
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

add_subdirectory(src)

install(EXPORT 2geom_targets
    FILE 2GeomTargets.cmake
    NAMESPACE 2Geom::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/2Geom"
    COMPONENT "lib2geom_dev"
    )

include(CMakePackageConfigHelpers)
write_basic_package_version_file("2GeomConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES "2GeomConfig.cmake" "${CMAKE_CURRENT_BINARY_DIR}/2GeomConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/2Geom"
    COMPONENT "lib2geom_dev"
    )

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/2geom"
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/2geom-${2GEOM_VERSION}"
	COMPONENT "lib2geom_dev")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/2geom.pc.in
                ${CMAKE_CURRENT_BINARY_DIR}/2geom.pc @ONLY IMMEDIATE )
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/2geom.pc"
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
	COMPONENT "lib2geom_dev")
