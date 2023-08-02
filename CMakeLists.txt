cmake_minimum_required(VERSION 3.24)
project(bm)

set(CMAKE_CXX_STANDARD 20)

find_package(nlohmann_json)
find_package(CURL REQUIRED)

add_library(bmcore OBJECT PortGroup.cpp PortGroup.h Ports.cpp Ports.h Port.cpp Port.h Build.cpp Build.h
        Distfile.cpp Distfile.h CurlEasy.cpp CurlEasy.h Fork.cpp Fork.h Exec.cpp Exec.h Shell.cpp Shell.h
        Buildenv.cpp Buildenv.h Unpack.cpp Unpack.h sha2alg.cpp sha2alg.h endian.h FindLib.cpp FindLib.h
        Sysconfig.cpp Sysconfig.h Mount.cpp Mount.h Chroot.cpp Chroot.h Db.cpp Db.h
        Installed.cpp Installed.h)

add_library(bmapp OBJECT App.cpp App.h)

add_executable(bm main.cpp)

target_include_directories(bm PRIVATE ${nlohmann_json_INCLUDE_DIRS})
target_include_directories(bm PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(bm bmapp)
target_link_libraries(bm bmcore)
target_link_libraries(bm ${CURL_LIBRARIES})

install(TARGETS bm DESTINATION bin)
