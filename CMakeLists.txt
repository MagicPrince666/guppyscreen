cmake_minimum_required(VERSION 3.10)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
project(guppyscreen LANGUAGES CXX C)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()

message(STATUS "System : ${CMAKE_SYSTEM_NAME}")
message(STATUS "Target system processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Host architecture: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

# 执行git命令，并把结果重定向到自定义变量（此处是我的变量<GIT_VERSION>）。
execute_process(
	COMMAND	git log -1 --format=%H 		
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} 
	OUTPUT_VARIABLE GIT_VERSION
)
execute_process(
	COMMAND git remote -v 			
	WORKING_DIRECTORY ${PROJECT_SOURCE_DIR} 
	OUTPUT_VARIABLE GIT_REMOTE_PATH
)

# 对得到的结果进行处理，尤其注意 \n,\t,\r之类的特殊字符，在cmake时没问题，但是生成的.cmake文件有问题，导致make出错。
string (REGEX REPLACE ".*\t| .*" "" GIT_PATH ${GIT_REMOTE_PATH})
string (REGEX REPLACE "[\n\t\r]" "" GIT_VERSION ${GIT_VERSION})

# 增加编译选项，把宏导入源码
add_definitions( -DGIT_VERSION=\"${GIT_VERSION}\")
add_definitions( -DGIT_PATH=\"${GIT_PATH}\")

# 获取工程路径
add_definitions(-DPROJECT_PATH="${PROJECT_SOURCE_DIR}")

set(COMMON_LIBRARIES
    pthread
    atomic
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  message(STATUS "${CMAKE_SYSTEM_PROCESSOR} mechine compile")
  add_definitions(-DLV_BUILD_TEST=0 -DSIMULATOR -D_GNU_SOURCE)
  find_package(SDL2 REQUIRED)
  list(APPEND COMMON_LIBRARIES ${SDL2_LIBRARIES} stdc++fs)
else()
if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
  # 交叉编译fb应用
  message(STATUS "cross compile")
  # list(APPEND COMMON_LIBRARIES ${OPENSSL_SSL_LIBRARY})
else()
  # 直接编译
  message(STATUS "host compile")
endif()
  list(APPEND COMMON_LIBRARIES stdc++fs)
endif()
add_definitions(-DCONFIG_ELOOP_EPOLL)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
set(CMAKE_CXX_STANDARD 17)
if (${MACHINES} MATCHES "arm64")
  message(STATUS "apple slicon compile")
  #include_directories(/opt/homebrew/include)
else()
  message(STATUS "intel compile")
  #include_directories(/usr/local/opt/openssl/include)
endif()
  add_definitions(-DLV_BUILD_TEST=0 -DSIMULATOR -D_GNU_SOURCE -DCONFIG_ELOOP_KQUEUE)
  find_package(SDL2 REQUIRED)
  # find_package(OpenSSL QUIET)
  list(APPEND COMMON_LIBRARIES 
    ${SDL2_LIBRARIES}
    # ${OPENSSL_SSL_LIBRARY}
    # ${OPENSSL_CRYPTO_LIBRARY}
  )
  include_directories(${SDL2_INCLUDE_DIRS})
  set(CMAKE_EXE_LINKER_FLAGS "-framework CoreFoundation -framework Security")
else()
  # windows not support
  message(STATUS "System : ${CMAKE_SYSTEM_NAME} not support")
endif()

# set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_BUILD_TYPE "Relese")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
add_definitions(-DBACKTRACE_DEBUG)
endif ()

add_definitions(-DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_UNIX -DSPDLOG_COMPILED_LIB)

include_directories(
  .
  src
  libhv
  libhv/base
  libhv/cpputil
  libhv/event
  libhv/evpp
  libhv/http
  libhv/http/client
  libhv/http/server
  libhv/ssl
  libhv/util
  spdlog/include
  wpa_supplicant/src
  wpa_supplicant/src/common
  wpa_supplicant/src/utils
  lvgl
  lv_touch_calibration
)

file(GLOB main_srcs 
    src/*.cpp
    assets/*.c
    assets/material/*.c
    lv_touch_calibration/*.c
    libhv/base/*.c
    libhv/cpputil/*.cpp
    libhv/event/*.c
    libhv/http/*.c
    libhv/http/*.cpp
    libhv/http/client/*.cpp
    libhv/http/server/*.cpp
    libhv/ssl/*.c
    libhv/util/*.c
    wpa_supplicant/src/common/wpa_ctrl.c
    wpa_supplicant/src/utils/os_unix.c
    wpa_supplicant/src/utils/wpa_debug.c
    wpa_supplicant/src/utils/common.c
    spdlog/src/*.cpp
)

add_subdirectory(lvgl)
add_subdirectory(lv_drivers)

add_executable(${PROJECT_NAME} ${main_srcs})
target_link_libraries(${PROJECT_NAME} PRIVATE
  lvgl
  lvgl::examples
  lvgl::demos
  lvgl::drivers
  ${COMMON_LIBRARIES}
)
