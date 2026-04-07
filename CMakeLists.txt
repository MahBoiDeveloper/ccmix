cmake_minimum_required(VERSION 3.10)

project(CCMIX)
set(CCMIX_MAJOR "0")
set(CCMIX_MINOR "1")
set(CCMIX_PATCH "0")
set(VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CCMIX_SOURCE_DIR}/cmake/modules)

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX /usr/local)
  set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
  set(CMAKE_INSTALL_DATADIR ${CMAKE_INSTALL_PREFIX}/share/ccmix)
  set(EASTWOOD_ROOTINSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

find_package(Crypto++ REQUIRED)
if(NOT CRYPTOPP_FOUND)
  message(FATAL_ERROR "libcrypto++ not found")
else()
  include_directories(${CRYPTOPP_INCLUDE_DIRS})
endif()

#make binary relocatable if we used shared libs
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib:$ORIGIN/")

set(PROJECT_SRC_DIR "src/")

if(NOT MSVC)
  set(WARN_FLAGS "-Wall -Wno-long-long -Wno-variadic-macros")
  set(WARN_FLAGS_DEBUG "-Wextra -Weffc++ -pedantic -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wreorder -Wabi -Wstrict-null-sentinel -Wc++0x-compat -Wformat=2 -Winit-self -Wswitch-default -Wsign-conversion -Wlogical-op -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Winline")
  # Don't enable these for now, but leave them around to investigate closer later
  set(WARN_FLAGS_DEBUG_EXCESSIVE "-Wpadded -Wunreachable-code -Wconversion")
  set(CMAKE_CXX_FLAGS "${WARN_FLAGS} -fPIC ${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${WARN_FLAGS_DEBUG} -g -O0 ${CMAKE_CXX_FLAGS_DEBUG}")
else()
  set(CMAKE_C_FLAGS_DEBUG_INIT "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
  set(CMAKE_C_FLAGS_MINSIZEREL_INIT     "/MT /O1 /Ob1 /D NDEBUG")
  set(CMAKE_C_FLAGS_RELEASE_INIT        "/MT /O2 /Ob2 /D NDEBUG")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "/MT /Zi /O2 /Ob1 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG_INIT "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
  set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "/MT /O1 /Ob1 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE_INIT        "/MT /O2 /Ob2 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MT /Zi /O2 /Ob1 /D NDEBUG")
endif()

set(SHARED_SRC
    ${PROJECT_SRC_DIR}/mix_db_gmd.cpp
    ${PROJECT_SRC_DIR}/mix_db_gamedb.cpp
    ${PROJECT_SRC_DIR}/mixid.cpp
   )

set(CCMIX_SRC
    ${PROJECT_SRC_DIR}/ccmix.cpp
    ${PROJECT_SRC_DIR}/mix_db_lmd.cpp
    ${PROJECT_SRC_DIR}/mix_file.cpp
    ${PROJECT_SRC_DIR}/mix_header.cpp
   )

set(GMDEDIT_SRC
    ${PROJECT_SRC_DIR}/gmdedit/gmdedit.cpp 
   )

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/bin)

add_executable(ccmix ${CCMIX_SRC} ${SHARED_SRC})
target_link_libraries(ccmix ${CRYPTOPP_LIBRARIES})

add_executable(gmdedit ${GMDEDIT_SRC} ${SHARED_SRC})
target_link_libraries(gmdedit)
