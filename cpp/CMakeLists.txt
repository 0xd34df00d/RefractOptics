CMAKE_MINIMUM_REQUIRED (VERSION 2.8)
PROJECT (optics)
SET (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++11 -pthread")

add_library (util STATIC
	util.cpp
	)

add_executable (optics WIN32
	main.cpp
	stats.cpp
	)

add_executable (test_linear WIN32
	testlinear_main.cpp
	)

add_executable (interpolator WIN32
	interpolator.cpp
	interpolate_main.cpp
	)

target_link_libraries (optics util)
target_link_libraries (interpolator gmp util)
