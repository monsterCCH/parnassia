# CMAKE 版本设置
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# CMAKE 版本策略设置
if(POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()

# 设置项目信息
set(PRO_MAJOR_VERSION "1")
set(PRO_MINOR_VERSION "0")
set(PRO_PATCH_VERSION "0")
set(PRO_VERSION_STRING "${PRO_MAJOR_VERSION}.${PRO_MINOR_VERSION}.${PRO_PATCH_VERSION}")

# 项目名称
project(parnassia_trinervis VERSION "${PRO_VERSION_STRING}" LANGUAGES CXX)

# 设置 CMAKE 依赖查找
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# 禁止在源码目录编译
if (${CMAKE_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR})
    message(FATAL_ERROR "In-source builds are not allowed")
endif()

# 输出编译器信息
message(STATUS "Configuring on: ${CMAKE_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

# 设置编译类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 打印cmake设置的编译标志
string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
message(STATUS "C flags configuration: ${CMAKE_C_FLAGS_${BUILD_TYPE}}")

# 设置语言标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 调试开关
set(CMAKE_VERBOSE_MAKEFILE 1)

# cmake_dependent_option依赖
include(CMakeDependentOption)

# 设置选项开关
# google test
option(GTEST "Build and run Gtest ..." ON)

# 设置编译器选项
set(COMPILER_FLAGS)
set(COMPILER_FLAGS_DEBUG)
set(COMPILER_FLAGS_RELEASE)
if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    list(APPEND CXX_FLAGS "-fno-rtti" )
    list(APPEND CXX_FLAGS_DEBUG "-Wsuggest-final-types" "-Wsuggest-final-methods" "-Wsuggest-override")
    list(APPEND CXX_FLAGS_RELEASE "-O3" "-Wno-unused")
endif()

# 设置 sanitizer 选项
include(CheckCXXCompilerFlag)
set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_REQUIRED_FLAGS ${ASAN_FLAGS})
check_cxx_compiler_flag(${ASAN_FLAGS} asan_work)
unset(CMAKE_REQUIRED_FLAGS)
if (asan_work)
    string(REPLACE " " ";" _asan_flags ${ASAN_FLAGS})
endif ()

# 如CONAN_LIBCXX未指定,直接添加add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
if(CONAN_LIBCXX STREQUAL "libstdc++11")
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
elseif(CONAN_LIBCXX STREQUAL "libstdc++")
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=0)
endif()

# 统一构建目标输出目录
#   静态库输出路径 => 构建目录/lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
#   动态库输出路径 => 构建目录/lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
#   可执行程序输出目录 => 构建目录/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

# 设置自定义第三方库路径 指定路径-DTHIRD_PARTY_LIB_BASE=XXX
if (NOT THIRD_PARTY_LIB_BASE)
    set(THIRD_PARTY_LIB_BASE "/opt/comm_lib")
endif (NOT THIRD_PARTY_LIB_BASE)
message(STATUS "-DTHIRD_PARTY_LIB_BASE=${THIRD_PARTY_LIB_BASE}")

# 设置依赖第三方库
set(THIRD_PARTY_LIBS
        ${THIRD_PARTY_LIB_BASE}/google_test
#        ${THIRD_PARTY_LIB_BASE}/hiredis_cluster
        ${THIRD_PARTY_LIB_BASE}/redis-plus-plus
        ${THIRD_PARTY_LIB_BASE}/nlohmann_json
        ${THIRD_PARTY_LIB_BASE}/spdlog
        ${THIRD_PARTY_LIB_BASE}/libevent
        )
set(THIRD_PARTY_LIBS_INC ${THIRD_PARTY_LIBS})
set(THIRD_PARTY_LIBS_LINK ${THIRD_PARTY_LIBS})

foreach(_item ${THIRD_PARTY_LIBS})
    message(STATUS "Add depend lib : ${_item}")
endforeach()

list(TRANSFORM THIRD_PARTY_LIBS_INC APPEND "/include")
list(TRANSFORM THIRD_PARTY_LIBS_LINK APPEND "/lib/linux/x86-64")

# 设置公共头文件路径
include_directories(${THIRD_PARTY_LIBS_INC})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/utils)

# 设置第三方依赖库
link_directories(${THIRD_PARTY_LIBS_LINK})

# 设置子模块目录
add_subdirectory(src)

#设置google test目录
if (GTEST AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    add_subdirectory(gtest)
endif ()
