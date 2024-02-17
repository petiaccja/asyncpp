add_executable(test)

target_sources(test
	PRIVATE
		container/test_atomic_collection.cpp
		container/test_atomic_item.cpp
		container/test_atomic_queue.cpp
		container/test_atomic_stack.cpp
		memory/test_rc_ptr.cpp
		main.cpp		
		test_generator.cpp
		test_join.cpp
		test_mutex.cpp
		test_shared_mutex.cpp
		test_shared_task.cpp
		test_stream.cpp
		test_task.cpp
		test_thread_pool.cpp
		test_event.cpp
		test_sleep.cpp
		testing/test_interleaver.cpp
)


find_package(Catch2 3 REQUIRED)
target_link_libraries(test Catch2::Catch2)
target_link_libraries(test asyncpp)