cmake_minimum_required(VERSION 3.6)
project(cs525_assign2_dbeniwal1)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    buffer_mgr.c
    buffer_mgr.h
    buffer_mgr_stat.c
    buffer_mgr_stat.h
    dberror.c
    dberror.h
    dt.h
    replacementStrategies.c
    storage_mgr.c
    storage_mgr.h
    test_assign2_1.c
    test_helper.h)

add_executable(cs525_assign2_dbeniwal1 ${SOURCE_FILES})