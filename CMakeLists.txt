cmake_minimum_required (VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)

project (SVISSR-Decoder)

file(GLOB_RECURSE GVAR-Decoder_CPPS src/*.cpp)

if(APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

add_executable(SVISSR-Decoder ${GVAR-Decoder_CPPS})

target_include_directories(SVISSR-Decoder PUBLIC src)

target_link_libraries(SVISSR-Decoder PUBLIC m)

find_package(Threads REQUIRED)
target_link_libraries(SVISSR-Decoder PUBLIC Threads::Threads)

if(WIN32 AND NOT MINGW)
    find_package(PNG CONFIG REQUIRED)
else()
    find_package(PNG REQUIRED)
endif()
target_link_libraries(SVISSR-Decoder PUBLIC PNG::PNG)

install(TARGETS SVISSR-Decoder DESTINATION bin)