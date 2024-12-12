cmake_minimum_required(VERSION 3.10)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
project(dreame_ui LANGUAGES CXX C)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 11)
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
    stdc++fs
    libhv.a
    libwpa_client.a
    libspdlog.a
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64")
  message(STATUS "${CMAKE_SYSTEM_PROCESSOR} mechine compile")
  add_definitions(-DLV_BUILD_TEST=0 -DSIMULATOR -D_GNU_SOURCE -DSPDLOG_COMPILED_LIB)
  find_package(SDL2 REQUIRED)
  list(APPEND COMMON_LIBRARIES ${SDL2_LIBRARIES})
else()
if (${CMAKE_HOST_SYSTEM_PROCESSOR} MATCHES "x86_64")
  # 交叉编译fb应用
  message(STATUS "cross compile")
else()
  # 直接编译
  message(STATUS "host compile")
endif()
endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
if (${MACHINES} MATCHES "arm64")
  message(STATUS "apple slicon compile")
else()
  message(STATUS "intel compile")
endif()
  find_package(SDL2 REQUIRED)
  list(APPEND COMMON_LIBRARIES ${SDL2_LIBRARIES})
  include_directories(${SDL2_INCLUDE_DIRS})
else()
  # windows not support
  message(STATUS "System : ${CMAKE_SYSTEM_NAME} not support")
endif()

# set(CMAKE_BUILD_TYPE "Debug")
if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
add_definitions(-DBACKTRACE_DEBUG)
endif ()

include_directories(
  .
  src
  libhv/include
  spdlog/include
  wpa_supplicant/src/common
  lvgl
  lv_touch_calibration
)

link_directories(
  libhv/lib
  spdlog/build
  wpa_supplicant/wpa_supplicant
)

file(GLOB main_srcs 
    src/*.cpp
    assets/*.c
    assets/material/*.c
    lv_touch_calibration/*.c
)

list(REMOVE_ITEM main_srcs src/kd_graphic_mode.cpp)

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
