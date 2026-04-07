# Module for locating the Crypto++ encryption library.
#
# Customizable variables:
#   CRYPTOPP_ROOT_DIR
#     This variable points to the CryptoPP root directory. On Windows the
#     library location typically will have to be provided explicitly using the
#     -D command-line option. The directory should include the include/cryptopp,
#     lib and/or bin sub-directories.
#
# Read-only variables:
#   CRYPTOPP_FOUND
#     Indicates whether the library has been found.
#
#   CRYPTOPP_INCLUDE_DIRS
#     Points to the CryptoPP include directory.
#
#   CRYPTOPP_LIBRARIES
#     Points to the CryptoPP libraries that should be passed to
#     target_link_libararies.
#
#
# Copyright (c) 2012 Sergiu Dotenco
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

include(FindPackageHandleStandardArgs)

find_path(CRYPTOPP_ROOT_DIR
  NAMES cryptopp/cryptlib.h include/cryptopp/cryptlib.h
  PATHS ENV CRYPTOPPROOT
  DOC "CryptoPP root directory")

# Re-use the previous path:
find_path(CRYPTOPP_INCLUDE_DIR
  NAMES cryptopp/cryptlib.h
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES include
  DOC "CryptoPP include directory")

find_library(CRYPTOPP_LIBRARY_DEBUG
  NAMES cryptlibd cryptoppd
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES lib
  DOC "CryptoPP debug library")

find_library(CRYPTOPP_LIBRARY_RELEASE
  NAMES cryptlib cryptopp
  HINTS ${CRYPTOPP_ROOT_DIR}
  PATH_SUFFIXES lib
  DOC "CryptoPP release library")

if(CRYPTOPP_LIBRARY_DEBUG AND CRYPTOPP_LIBRARY_RELEASE)
  set(CRYPTOPP_LIBRARY
    optimized ${CRYPTOPP_LIBRARY_RELEASE}
    debug ${CRYPTOPP_LIBRARY_DEBUG} CACHE DOC "CryptoPP library")
elseif(CRYPTOPP_LIBRARY_RELEASE)
  set(CRYPTOPP_LIBRARY ${CRYPTOPP_LIBRARY_RELEASE} CACHE DOC
    "CryptoPP library")
endif()

if(CRYPTOPP_INCLUDE_DIR)
  set(_CRYPTOPP_VERSION_HEADER ${CRYPTOPP_INCLUDE_DIR}/cryptopp/config.h)

  if(EXISTS ${_CRYPTOPP_VERSION_HEADER})
    file(STRINGS ${_CRYPTOPP_VERSION_HEADER} _CRYPTOPP_VERSION_TMP REGEX
      "^#define CRYPTOPP_VERSION[ \t]+[0-9]+$")

    string(REGEX REPLACE
      "^#define CRYPTOPP_VERSION[ \t]+([0-9]+)" "\\1" _CRYPTOPP_VERSION_TMP
      ${_CRYPTOPP_VERSION_TMP})

    string(REGEX REPLACE "([0-9]+)[0-9][0-9]" "\\1" CRYPTOPP_VERSION_MAJOR
      ${_CRYPTOPP_VERSION_TMP})
    string(REGEX REPLACE "[0-9]([0-9])[0-9]" "\\1" CRYPTOPP_VERSION_MINOR
      ${_CRYPTOPP_VERSION_TMP})
    string(REGEX REPLACE "[0-9][0-9]([0-9])" "\\1" CRYPTOPP_VERSION_PATCH
      ${_CRYPTOPP_VERSION_TMP})

    set(CRYPTOPP_VERSION_COUNT 3)
    set(CRYPTOPP_VERSION
      ${CRYPTOPP_VERSION_MAJOR}.${CRYPTOPP_VERSION_MINOR}.${CRYPTOPP_VERSION_PATCH})
  endif()
endif()

set(CRYPTOPP_INCLUDE_DIRS ${CRYPTOPP_INCLUDE_DIR})
set(CRYPTOPP_LIBRARIES ${CRYPTOPP_LIBRARY})

mark_as_advanced(CRYPTOPP_INCLUDE_DIR CRYPTOPP_LIBRARY CRYPTOPP_LIBRARY_DEBUG
  CRYPTOPP_LIBRARY_RELEASE)

find_package_handle_standard_args("Crypto++" REQUIRED_VARS CRYPTOPP_ROOT_DIR
  CRYPTOPP_INCLUDE_DIR CRYPTOPP_LIBRARY VERSION_VAR CRYPTOPP_VERSION)
