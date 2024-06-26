# Copyright (c) 2018-2020 Ribose Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

if(MSVC)
  # remove extra ${Configuration} subfolder
  set(ArchiveOutputDir ${CMAKE_BINARY_DIR}\\src\\rnp)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${ArchiveOutputDir})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${ArchiveOutputDir})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${ArchiveOutputDir})
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${ArchiveOutputDir})

  set(RuntimeOutputDir ${CMAKE_BINARY_DIR}\\src\\rnp)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${RuntimeOutputDir})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${RuntimeOutputDir})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${RuntimeOutputDir})
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${RuntimeOutputDir})

  find_path(GETOPT_INCLUDE_DIR
    NAMES getopt.h
  )
  find_library(GETOPT_LIBRARY
    NAMES getopt
  )
  find_path(DIRENT_INCLUDE_DIR
    NAMES dirent.h
  )
endif()

# for the headers
find_package(JSON-C 0.11 REQUIRED)

add_executable(rnp
  rnp.cpp
  fficli.cpp
  rnpcfg.cpp
  ../rnpkeys/tui.cpp
)

if(BUILD_SHARED_LIBS)
  target_sources(rnp PRIVATE ../lib/logging.cpp $<TARGET_OBJECTS:rnp-common>)

  if(APPLE)
    # Since cmd line tools version 2397 dyld doesn't look in ../lib
    # See issue #2153 and https://developer.apple.com/forums/thread/737920 for the details.
    set_target_properties(rnp PROPERTIES
      INSTALL_RPATH "@executable_path/../lib"
    )
  endif(APPLE)
endif(BUILD_SHARED_LIBS)

target_include_directories(rnp
  PRIVATE
    "${PROJECT_SOURCE_DIR}/src"
    "${PROJECT_SOURCE_DIR}/src/lib"
    "${JSON-C_INCLUDE_DIRS}"
)
if(MSVC)
  target_include_directories(rnp
    PRIVATE
      "${GETOPT_INCLUDE_DIR}"
      "${DIRENT_INCLUDE_DIR}"
    )
endif()

target_link_libraries(rnp
  PRIVATE
    librnp
    JSON-C::JSON-C
)
if(MSVC)
  target_link_libraries(rnp
    PRIVATE
      "${GETOPT_LIBRARY}"
    )
endif(MSVC)

include(GNUInstallDirs)
install(TARGETS rnp
  RUNTIME
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT cli
)

# Build and install man page
if (ENABLE_DOC)
  add_adoc_man("${CMAKE_CURRENT_SOURCE_DIR}/rnp.1.adoc" ${RNP_VERSION})
endif()
