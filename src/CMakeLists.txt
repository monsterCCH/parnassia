# CMAKE 版本设置
cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# 项目名称
project(parnassia)

# 设置项目头文件

# 设置项目源文件
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SOURCES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/../utils UTILS_SOURCES)

# 设置链接库路劲
link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

# 设置生成目标
add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${UTILS_SOURCES}
        )

set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE 1)

# 设置编译器属性
target_compile_features(${PROJECT_NAME}
        PRIVATE
            cxx_std_11
        )

# 设置编译选项
target_compile_options(${PROJECT_NAME}
        PRIVATE
        "-gdwarf-4"
        #        "-fsanitize=address" "-fno-omit-frame-pointer"
        "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
        "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>"
        )

#设置链接目标
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        libiniparser.a
        libhiredis.a
        libevent.a
        libredis++.a
        libssh2.a
        libworkflow.a
        libssl.a
        libcrypto.a
        pthread
        dl
        #        -fsanitize=address
        )
