cmake_minimum_required (VERSION 2.6)
project (libpedalog)

add_library(pedalog SHARED src/pedalog.c src/pedalog.h)
target_link_libraries(pedalog usb)

# The version number.
set (LIBPEDALOG_VERSION_MAJOR 0)
set (LIBPEDALOG_VERSION_MINOR 1)

install (TARGETS pedalog DESTINATION lib)
install (FILES src/pedalog.h DESTINATION include)
