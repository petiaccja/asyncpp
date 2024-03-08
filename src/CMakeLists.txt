add_library(asyncpp)

target_sources(asyncpp
	PRIVATE
		thread_pool.cpp
		mutex.cpp
		shared_mutex.cpp
		sleep.cpp
		testing/interleaver.cpp
		semaphore.cpp
)

target_link_libraries(asyncpp asyncpp-headers)