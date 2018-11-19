# THIS FILE IS ONLY FOR DEVELOPMENT!
# USE ./build.sh TO BUILD A STATIC RELEASE FOR LINUX DISTRIBUTIONS.

cmake_minimum_required(VERSION 3.12)
project(ml_gridengine_executor)

# setup Poco libraries
set(POCO_PREFIX "/usr/local")
set(POCO_INCLUDE_DIR "${POCO_PREFIX}/include")
set(POCO_LIB_DIR "${POCO_PREFIX}/lib")
set(POCO_LIBS PocoNet PocoUtil PocoFoundation)

# setup compiler options
set(CMAKE_CXX_STANDARD 11)
include_directories("${POCO_INCLUDE_DIR}")
link_directories("${POCO_LIB_DIR}")

# the main executable program
add_library(
        ml_gridengine_executor_lib
        src/macros.h
        src/version.h
        src/AutoFreePtr.h
        src/OutputBuffer.cpp
        src/OutputBuffer.h
        src/ProgramExecutor.cpp
        src/ProgramExecutor.h
        src/WebServerFactory.cpp
        src/WebServerFactory.h
        src/Logger.cpp
        src/Logger.h
        src/BaseApp.cpp
        src/BaseApp.h
        src/FormatUtils.cpp
        src/FormatUtils.h
        src/IOController.cpp
        src/IOController.h)
add_executable(ml_gridengine_executor main.cpp)
target_link_libraries(ml_gridengine_executor "${POCO_LIBS}" ml_gridengine_executor_lib)
