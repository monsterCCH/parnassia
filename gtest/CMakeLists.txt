#cmake版本
cmake_minimum_required(VERSION 3.15)

#测试项目名
project(parnassia_test)

#设置项目头文件
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/tests)

#设置项目源文件
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/tests TEST_FILES)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/mocks MOCK_FILES)
set(MODULE_FILE
        )

#链接生成静态库
link_directories(${CMAKE_ARCHIVE_OUTPUT_DIRECTORY})

#cmake启用测试
enable_testing()

#设置生成目标
add_executable(${PROJECT_NAME}
        ${TEST_FILES}
        ${MOCK_FILES}
        )

#设置编译属性
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

#设置编译选项
target_compile_options(${PROJECT_NAME}
        PRIVATE
        "$<$<CONFIG:Debug>:${CXX_FLAGS_DEBUG}>"
        "$<$<CONFIG:Release>:${CXX_FLAGS_RELEASE}>"
        )

#设置链接目标
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        gtest
        gtest_main
        pthread
        )

#自动查找测试项目
include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})
