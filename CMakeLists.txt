# USE ./build.sh TO BUILD A STATIC RELEASE FOR LINUX DISTRIBUTIONS.

cmake_minimum_required(VERSION 3.10)
project(ml_gridengine_executor)

# setup Poco libraries
set(POCO_PREFIX "/usr/local")
set(POCO_INCLUDE_DIR "${POCO_PREFIX}/include")
set(POCO_LIB_DIR "${POCO_PREFIX}/lib")
set(POCO_LIBS PocoCrypto PocoNet PocoZip PocoUtil PocoXML PocoJSON PocoFoundation)
set(POCO_DEP_LIBS pthread)

# setup catch2
include_directories("${PROJECT_SOURCE_DIR}/3rdparty")

# setup compiler options
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
include_directories("${POCO_INCLUDE_DIR}")
link_directories("${POCO_LIB_DIR}")

# the main sources
add_library(
        ml-gridengine-executor-sources
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
        src/Utils.cpp
        src/Utils.h
        src/IOController.cpp
        src/IOController.h
        src/CallbackAPI.cpp
        src/CallbackAPI.h
        src/GeneratedFilesWatcher.cpp
        src/GeneratedFilesWatcher.h
)

# the main executable program
add_executable(ml-gridengine-executor src/main.cpp)
target_link_libraries(ml-gridengine-executor ml-gridengine-executor-sources "${POCO_LIBS}" "${POCO_DEP_LIBS}")

# the tests
include_directories("${PROJECT_SOURCE_DIR}")
add_executable(
        ml-gridengine-executor-unit-tests
        tests/unit-tests/main.cpp
        tests/unit-tests/Utils.test.cpp tests/unit-tests/macros.h)
target_link_libraries(ml-gridengine-executor-unit-tests ml-gridengine-executor-sources "${POCO_LIBS}" "${POCO_DEP_LIBS}")
