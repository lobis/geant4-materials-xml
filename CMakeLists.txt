cmake_minimum_required(VERSION 3.7)

project(geant4-materials-xml)

set(CMAKE_CXX_STANDARD 17)

find_package(ROOT REQUIRED)
find_package(Geant4 REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}
        ${ROOT_LIBRARIES} ROOT::XMLIO
        ${Geant4_LIBRARIES}
        )

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
        ${ROOT_INCLUDE_DIRS}
        ${Geant4_INCLUDE_DIR}
        )

target_compile_options(${PROJECT_NAME} PRIVATE)
