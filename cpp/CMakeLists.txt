CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (optics)
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++0x -pthread")

ADD_EXECUTABLE (optics WIN32
	main.cpp
	stats.cpp
	)

add_executable (test_linear WIN32
	testlinear_main.cpp
	)
