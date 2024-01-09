add_subdirectory(libuemf)
add_subdirectory(libcroco)
add_subdirectory(libdepixelize)
add_subdirectory(adaptagrams)
add_subdirectory(autotrace)

if(WITH_INTERNAL_2GEOM)
    set(2GEOM_BUILD_SHARED ${BUILD_SHARED_LIBS} CACHE BOOL "")
    add_subdirectory(2geom)
endif()

if(WITH_INTERNAL_CAIRO)
    include(ExternalProject)
    # Used by windows CI/CD/packaging, Linux appimages

    # Change, don't remove options.
    # Otherwise the values are not reset in existing builds.
    set(CAIRO_CONF --buildtype=debugoptimized -Dgtk_doc=false)
    if(WIN32)
        set(CAIRO_CONF ${CAIRO_CONF} -Dfreetype=enabled -Dfontconfig=enabled)
    else()
        set(CAIRO_CONF ${CAIRO_CONF} --prefix=${CMAKE_INSTALL_PREFIX})
    endif()

    ExternalProject_Add(cairo_ext
        PREFIX cairo
        GIT_REPOSITORY https://gitlab.com/inkscape/deps/cairo.git
        GIT_TAG 1.18.0
        GIT_SHALLOW TRUE
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cairo
        CONFIGURE_COMMAND meson setup . ${CMAKE_CURRENT_SOURCE_DIR}/cairo && meson setup --reconfigure ${CAIRO_CONF} . ${CMAKE_CURRENT_SOURCE_DIR}/cairo
        BUILD_COMMAND meson compile
        INSTALL_COMMAND meson install
        )
endif()
