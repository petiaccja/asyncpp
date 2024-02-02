add_library(asyncpp)

target_sources(asyncpp
	PRIVATE
		thread_pool.cpp
		mutex.cpp
		shared_mutex.cpp
		sleep.cpp
		interleaving/runner.cpp
		interleaving/sequencer.cpp
		interleaving/state_tree.cpp
)

target_link_libraries(asyncpp asyncpp-headers)