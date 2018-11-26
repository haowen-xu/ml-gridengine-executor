# USE ./scripts/build.sh TO BUILD A STATIC RELEASE FOR LINUX DISTRIBUTIONS.

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
include_directories("${POCO_INCLUDE_DIR}" "${PROJECT_SOURCE_DIR}")
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
        src/GeneratedFilesWatcher.cpp
        src/GeneratedFilesWatcher.h
        src/PersistAndCallbackManager.cpp
        src/PersistAndCallbackManager.h
        src/HTTPError.cpp
        src/HTTPError.h
        src/SignalHandler.cpp
        src/SignalHandler.h
)

# the main executable program
add_executable(ml-gridengine-executor src/main.cpp)
target_link_libraries(ml-gridengine-executor ml-gridengine-executor-sources "${POCO_LIBS}" "${POCO_DEP_LIBS}")

# the tests
add_executable(
        ml-gridengine-executor-unit-tests
        tests/unit-tests/main.cpp
        tests/unit-tests/macros.h
        tests/unit-tests/CapturingLogger.cpp
        tests/unit-tests/CapturingLogger.h
        tests/unit-tests/Utils.test.cpp
        tests/unit-tests/OutputBuffer.test.cpp
        tests/unit-tests/ProgramExecutor.test.cpp
        tests/unit-tests/SignalHandler.test.cpp)
target_link_libraries(ml-gridengine-executor-unit-tests ml-gridengine-executor-sources "${POCO_LIBS}" "${POCO_DEP_LIBS}")

# the test assets
add_executable(
        Count
        tests/assets/Count.cpp)
add_executable(
        SignalHandlerExample
        tests/assets/SignalHandlerExample.cpp)
target_link_libraries(SignalHandlerExample ml-gridengine-executor-sources "${POCO_LIBS}" "${POCO_DEP_LIBS}")
