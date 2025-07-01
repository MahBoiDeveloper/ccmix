CMAKE_MINIMUM_REQUIRED(VERSION 3.15 FATAL_ERROR)

SET(MAIN_TARGET "ccmix")

PROJECT(
    ${MAIN_TARGET}
    VERSION "0.2.0"
    LANGUAGE CXX
)

SET(CMAKE_CXX_STANDARD 20)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(PROJECT_SRC_DIR "src/")

IF(NOT MSVC)
  SET(WARN_FLAGS "-Wall -Wno-long-long -Wno-variadic-macros")
  SET(WARN_FLAGS_DEBUG "-Wextra -Weffc++ -pedantic -Wold-style-cast -Woverloaded-virtual -Wctor-dtor-privacy -Wreorder -Wabi -Wstrict-null-sentinel -Wc++0x-compat -Wformat=2 -Winit-self -Wswitch-default -Wsign-conversion -Wlogical-op -Wmissing-declarations -Wmissing-noreturn -Wmissing-format-attribute -Wredundant-decls -Winline")
  # Don't enable these for now, but leave them around to investigate closer later
  SET(WARN_FLAGS_DEBUG_EXCESSIVE "-Wpadded -Wunreachable-code -Wconversion")
  SET(CMAKE_CXX_FLAGS "${WARN_FLAGS} -fPIC ${CMAKE_CXX_FLAGS}")
  SET(CMAKE_CXX_FLAGS_DEBUG "${WARN_FLAGS_DEBUG} -g -O0 ${CMAKE_CXX_FLAGS_DEBUG}")
ELSE(NOT MSVC)
  SET(CMAKE_C_FLAGS_DEBUG_INIT            "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
  SET(CMAKE_C_FLAGS_MINSIZEREL_INIT       "/MT /O1 /Ob1 /D NDEBUG")
  SET(CMAKE_C_FLAGS_RELEASE_INIT          "/MT /O2 /Ob2 /D NDEBUG")
  SET(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT   "/MT /Zi /O2 /Ob1 /D NDEBUG")
  SET(CMAKE_CXX_FLAGS_DEBUG_INIT          "/D_DEBUG /MTd /Zi /Ob0 /Od /RTC1")
  SET(CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "/MT /O1 /Ob1 /D NDEBUG")
  SET(CMAKE_CXX_FLAGS_RELEASE_INIT        "/MT /O2 /Ob2 /D NDEBUG")
  SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "/MT /Zi /O2 /Ob1 /D NDEBUG")
ENDIF(NOT MSVC)

SET(SHARED_SRC
    ${PROJECT_SRC_DIR}/mix_db_gmd.cpp
    ${PROJECT_SRC_DIR}/mix_db_gamedb.cpp
    ${PROJECT_SRC_DIR}/mixid.cpp
)

SET(CCMIX_SRC
    ${PROJECT_SRC_DIR}/ccmix.cpp
    ${PROJECT_SRC_DIR}/mix_db_lmd.cpp
    ${PROJECT_SRC_DIR}/mix_file.cpp
    ${PROJECT_SRC_DIR}/mix_header.cpp
)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CCMIX_BINARY_DIR}/bin)

ADD_EXECUTABLE(${MAIN_TARGET})

TARGET_SOURCES(
    ${MAIN_TARGET}
    PRIVATE
    ${CCMIX_SRC}
    ${SHARED_SRC}
)

TARGET_LINK_LIBRARIES(${MAIN_TARGET} cryptopp)
