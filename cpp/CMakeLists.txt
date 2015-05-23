cmake_minimum_required (VERSION 2.8)
project (optics)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++1y")

find_package (Threads)

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

target_link_libraries (optics util ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries (interpolator gmp util ${CMAKE_THREAD_LIBS_INIT})
