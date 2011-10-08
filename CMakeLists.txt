cmake_minimum_required (VERSION 2.6)
project (libpedalog)

add_library(pedalog SHARED src/pedalog.c src/pedalog.h)
target_link_libraries(pedalog usb)

# The version number.
set (LIBPEDALOG_VERSION_MAJOR 0)
set (LIBPEDALOG_VERSION_MINOR 4)
set (LIBPEDALOG_VERSION_PATCH 0)

# Library versioning
set (serial "${LIBPEDALOG_VERSION_MAJOR}.${LIBPEDALOG_VERSION_MINOR}.${LIBPEDALOG_VERSION_PATCH}")
set_target_properties(pedalog PROPERTIES VERSION ${LIBPEDALOG_VERSION_MAJOR}.${LIBPEDALOG_VERSION_MINOR} SOVERSION ${LIBPEDALOG_VERSION_MAJOR})

install (TARGETS pedalog DESTINATION lib)
install (FILES src/pedalog.h DESTINATION include)

# Install udev rule so any user can access Pedalog devices
install (
	DIRECTORY	udev/rules/
	DESTINATION /etc/udev/rules.d
)

# Create 'dist' target to build source tarball
set(PROJECT_VERSION "${LIBPEDALOG_VERSION_MAJOR}.${LIBPEDALOG_VERSION_MINOR}")
set(ARCHIVE_NAME ${CMAKE_PROJECT_NAME}-${PROJECT_VERSION})
add_custom_target(dist
    COMMAND git archive --prefix=${ARCHIVE_NAME}/ HEAD
        | gzip > ${CMAKE_BINARY_DIR}/${ARCHIVE_NAME}.tar.gz
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
