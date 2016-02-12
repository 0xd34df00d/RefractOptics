cmake_minimum_required (VERSION 2.8)
project (optics)
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -std=c++1y")

find_package (Threads REQUIRED)
find_package (Boost REQUIRED COMPONENTS program_options system)

#set (DLIB_USE_BLAS True)
#set (DLIB_USE_LAPACK True)
#add_subdirectory (/usr/include/dlib dlib)
add_library (util STATIC
	util.cpp
	symbregmodels.cpp
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

target_link_libraries (optics
	util
	#dlib
	${CMAKE_THREAD_LIBS_INIT}
	${Boost_PROGRAM_OPTIONS_LIBRARY}
	${Boost_SYSTEM_LIBRARY}
	)
target_link_libraries (interpolator gmp util ${CMAKE_THREAD_LIBS_INIT})
