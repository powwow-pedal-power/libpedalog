cmake_minimum_required (VERSION 2.6)
project (libpedalog)

add_library(pedalog SHARED src/pedalog.c src/pedalog.h)
target_link_libraries(pedalog usb)

# The version number.
set (LIBPEDALOG_VERSION_MAJOR 0)
set (LIBPEDALOG_VERSION_MINOR 1)
SET (LIBPEDALOG_VERSION_PATCH 1)

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

# Debian packaging
#set(CPACK_GENERATOR "DEB")
#set(CPACK_DEBIAN_PACKAGE_NAME "libpedalog")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libusb-0.1-4 (>=2:0.1.12-17)")
#set(CPACK_DEBIAN_PACKAGE_MAINTAINER "dan@powwow-pedal-power.org.uk")
#SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Library to read data from Pedalog devices")
#SET(CPACK_PACKAGE_VENDOR "PoWWow Pedal Power")
#SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
#SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
#SET(CPACK_PACKAGE_VERSION_MAJOR "0")
#SET(CPACK_PACKAGE_VERSION_MINOR "1")
#SET(CPACK_PACKAGE_VERSION_PATCH "0")
#INCLUDE(CPack)

