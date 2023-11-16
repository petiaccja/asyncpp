add_executable(test)

target_sources(test
	PRIVATE
		main.cpp		
		test_generator.cpp
		test_stream.cpp
		test_thread_pool.cpp
		test_task.cpp
		test_atomic_stack.cpp
		interleaving/test_runner.cpp
		test_shared_task.cpp
		test_atomic_collection.cpp
)


find_package(Catch2 3 REQUIRED)
target_link_libraries(test Catch2::Catch2)
target_link_libraries(test async++)