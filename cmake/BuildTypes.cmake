set(AVAILABLE_BUILD_TYPES "Debug;Release;RelWithDebInfo;MinSizeRel;LTO;CodeCoverage;ASAN;TSAN")
if (NOT CMAKE_BUILD_TYPE IN_LIST AVAILABLE_BUILD_TYPES)
  message(
    FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}. Please specify one of the following: ${AVAILABLE_BUILD_TYPES}"
  )
endif ()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g1 -O3 -DNDEBUG)")
set(CMAKE_CXX_FLAGS_RELEASE "-g0 -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_LTO "${CMAKE_CXX_FLAGS_RELEASE} -flto")
set(CMAKE_CXX_FLAGS_CODECOVERAGE "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_ASAN "-O1 -g -fsanitize=address -fno-omit-frame-pointer ")
set(CMAKE_CXX_FLAGS_TSAN "-O1 -g -fsanitize=thread")

if (is_gcc)
  if (CMAKE_BUILD_TYPE STREQUAL "ASAN")
    target_link_libraries(clio_options INTERFACE asan)
  elseif (CMAKE_BUILD_TYPE STREQUAL "TSAN")
    target_link_libraries(clio_options INTERFACE tsan)
  endif ()
endif ()
