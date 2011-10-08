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
