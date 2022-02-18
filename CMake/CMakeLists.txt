cmake_minimum_required(VERSION 3.5)


project(pingg)

set(CMAKE_CXX_STANDART 11)

file(GLOB
	CPPS "*.cpp")

add_executable(${PROJECT_NAME} ${CPPS}) 

target_link_libraries(${PROJECT_NAME} wsock32 ws2_32)