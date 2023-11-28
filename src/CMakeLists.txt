add_library(async++)

target_sources(async++
	PRIVATE
		thread_pool.cpp
		mutex.cpp
		shared_mutex.cpp
		sleep.cpp
		interleaving/runner.cpp
		interleaving/sequencer.cpp
		interleaving/state_tree.cpp
)

target_link_libraries(async++ async++-headers)