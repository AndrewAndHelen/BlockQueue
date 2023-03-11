cmake_minimum_required(VERSION 3.18)

find_path(TASKFLOW_INCLUDE_DIR taskflow.hpp)
get_filename_component(TASKFLOW_INCLUDE_DIR ${TASKFLOW_INCLUDE_DIR} DIRECTORY)

set(BLOCK_QUEUE_TEST_SRCS 
	${CMAKE_CURRENT_SOURCE_DIR}/AsyncTaskExecutor.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/AsyncTaskExecutor.h
	${CMAKE_CURRENT_SOURCE_DIR}/block_queue_test.cpp

)
add_executable(BLOCK_QUEUE_TEST ${BLOCK_QUEUE_TEST_SRCS})
target_include_directories(BLOCK_QUEUE_TEST PUBLIC ${BLOCKQUEUE_PATH} ${TASKFLOW_INCLUDE_DIR})
target_compile_features(BLOCK_QUEUE_TEST PUBLIC cxx_std_17)