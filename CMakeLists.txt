cmake_minimum_required (VERSION 3.0.0)

set(CMAKE_CXX_STANDARD 17)

project (SVISSR-Ingestor)
file(GLOB_RECURSE SVISSR-Ingestor_CPPS src/*.cpp src/pipe.c)

if(APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

add_executable(SVISSR-Ingestor ${SVISSR-Ingestor_CPPS})

target_include_directories(SVISSR-Ingestor PUBLIC src)

target_link_libraries(SVISSR-Ingestor PUBLIC m)

find_package(Threads REQUIRED)
target_link_libraries(SVISSR-Ingestor PUBLIC Threads::Threads)

find_package(SoapySDR "0.6.0" NO_MODULE REQUIRED)
include_directories(${SOAPY_SDR_INCLUDE_DIR})
target_link_libraries(SVISSR-Ingestor PUBLIC ${SOAPY_SDR_LIBRARY})  

find_package(yaml-cpp REQUIRED)
target_include_directories(SVISSR-Ingestor PUBLIC ${YAML_CPP_INCLUDE_DIR})
if(MINGW)
    target_link_libraries(SVISSR-Ingestor PUBLIC ${YAML_CPP_LIBRARIES}"yaml-cpp")
else()
    target_link_libraries(SVISSR-Ingestor PUBLIC "yaml-cpp")
endif()

find_library(DSP_LIBRARY dsp REQUIRED)
target_link_libraries(SVISSR-Ingestor PUBLIC ${DSP_LIBRARY})

if(WIN32 AND NOT MINGW)
    find_package(PNG CONFIG REQUIRED)
else()
    find_package(PNG REQUIRED)
endif()
target_link_libraries(SVISSR-Ingestor PUBLIC PNG::PNG)

install(TARGETS SVISSR-Ingestor DESTINATION bin)