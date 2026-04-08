cmake_minimum_required(VERSION 3.10)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(CCMIX)
set(CCMIX_MAJOR "0")
set(CCMIX_MINOR "1")
set(CCMIX_PATCH "0")
set(VERSION "0.1.0")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH ${CCMIX_SOURCE_DIR}/cmake/modules)
set(CRYPTOPP_SUBMODULE_DIR ${CCMIX_SOURCE_DIR}/libs/cryptopp)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX /usr/local)
  set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
  set(CMAKE_INSTALL_DATADIR ${CMAKE_INSTALL_PREFIX}/share/ccmix)
  set(EASTWOOD_ROOTINSTALL_DIR ${CMAKE_INSTALL_PREFIX})
endif()

if(MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio" AND EXISTS "${CRYPTOPP_SUBMODULE_DIR}/cryptlib.vcxproj")
  message(STATUS "Using vendored Crypto++ from ${CRYPTOPP_SUBMODULE_DIR}")

  file(TO_NATIVE_PATH "${CCMIX_BINARY_DIR}" CCMIX_BINARY_DIR_NATIVE)
  set(CRYPTOPP_VS_CONFIGURATION "$<IF:$<CONFIG:Debug>,Debug,Release>")
  set(CRYPTOPP_OUTPUT_DIR_NATIVE "${CCMIX_BINARY_DIR_NATIVE}\\lib\\$<CONFIG>\\")
  set(CRYPTOPP_INTERMEDIATE_DIR_NATIVE "${CCMIX_BINARY_DIR_NATIVE}\\cryptopp\\${CMAKE_VS_PLATFORM_NAME}\\$<CONFIG>\\")

  add_library(cryptopp_dependency STATIC IMPORTED GLOBAL)
  set_target_properties(cryptopp_dependency PROPERTIES
    IMPORTED_CONFIGURATIONS "Debug;Release;RelWithDebInfo;MinSizeRel"
    IMPORTED_LOCATION_DEBUG "${CCMIX_BINARY_DIR}/lib/Debug/cryptlib.lib"
    IMPORTED_LOCATION_RELEASE "${CCMIX_BINARY_DIR}/lib/Release/cryptlib.lib"
    IMPORTED_LOCATION_RELWITHDEBINFO "${CCMIX_BINARY_DIR}/lib/RelWithDebInfo/cryptlib.lib"
    IMPORTED_LOCATION_MINSIZEREL "${CCMIX_BINARY_DIR}/lib/MinSizeRel/cryptlib.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CCMIX_SOURCE_DIR}/libs"
  )

  add_custom_target(cryptopp_build
    COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" "${CRYPTOPP_SUBMODULE_DIR}/cryptlib.vcxproj" /m
      "/p:Configuration=${CRYPTOPP_VS_CONFIGURATION}"
      "/p:Platform=${CMAKE_VS_PLATFORM_NAME}"
      "/p:OutDir=${CRYPTOPP_OUTPUT_DIR_NATIVE}"
      "/p:IntDir=${CRYPTOPP_INTERMEDIATE_DIR_NATIVE}"
    WORKING_DIRECTORY "${CRYPTOPP_SUBMODULE_DIR}"
    COMMENT "Building vendored Crypto++ static library"
    VERBATIM
  )
else()
  add_library(cryptopp_dependency INTERFACE)
  target_include_directories(cryptopp_dependency INTERFACE ${CRYPTOPP_INCLUDE_DIRS})
  target_link_libraries(cryptopp_dependency INTERFACE ${CRYPTOPP_LIBRARIES})
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
  set(CMAKE_C_FLAGS_DEBUG_INIT "/D_DEBUG /Zi /Ob0 /Od /RTC1")
  set(CMAKE_C_FLAGS_MINSIZEREL_INIT     "/O1 /Ob1 /D NDEBUG")
  set(CMAKE_C_FLAGS_RELEASE_INIT        "/O2 /Ob2 /D NDEBUG")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "/Zi /O2 /Ob1 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG_INIT "/D_DEBUG /Zi /Ob0 /Od /RTC1")
  set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "/O1 /Ob1 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE_INIT        "/O2 /Ob2 /D NDEBUG")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/Zi /O2 /Ob1 /D NDEBUG")
endif()

set(SHARED_SRC
    ${PROJECT_SRC_DIR}/mix_gmd.cpp
    ${PROJECT_SRC_DIR}/mix_game_db.cpp
    ${PROJECT_SRC_DIR}/mixid.cpp
   )

set(CCMIX_SRC
    ${PROJECT_SRC_DIR}/ccmix.cpp
    ${PROJECT_SRC_DIR}/mix_lmd.cpp
    ${PROJECT_SRC_DIR}/mix_file.cpp
    ${PROJECT_SRC_DIR}/mix_header.cpp
   )

set(GMDEDIT_SRC
    ${PROJECT_SRC_DIR}/gmdedit/gmdedit.cpp 
   )

set(WWMIX_SRC
    ${PROJECT_SRC_DIR}/wwmix/archive_cli.cpp
    ${PROJECT_SRC_DIR}/wwmix/main.cpp
    ${PROJECT_SRC_DIR}/wwmix/mix_game_db.cpp
    ${PROJECT_SRC_DIR}/wwmix/mix_gmd.cpp
    ${PROJECT_SRC_DIR}/wwmix/mix_lmd.cpp
    ${PROJECT_SRC_DIR}/wwmix/mix_file.cpp
    ${PROJECT_SRC_DIR}/wwmix/mix_header.cpp
    ${PROJECT_SRC_DIR}/wwmix/mixid.cpp
    ${PROJECT_SRC_DIR}/wwmix/wwmix.cpp
   )

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/bin)

add_executable(ccmix ${CCMIX_SRC} ${SHARED_SRC})
if(TARGET cryptopp_build)
  add_dependencies(ccmix cryptopp_build)
endif()
target_link_libraries(ccmix cryptopp_dependency)

add_executable(gmdedit ${GMDEDIT_SRC} ${SHARED_SRC})

add_executable(wwmix ${WWMIX_SRC})
if(TARGET cryptopp_build)
  add_dependencies(wwmix cryptopp_build)
endif()
target_link_libraries(wwmix cryptopp_dependency)
