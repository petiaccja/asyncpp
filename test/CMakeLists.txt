add_executable(test)

target_sources(test
	PRIVATE
		container/test_atomic_collection.cpp
		container/test_atomic_item.cpp		
		container/test_atomic_stack.cpp
		container/test_atomic_deque.cpp
		memory/test_rc_ptr.cpp
		main.cpp		
		test_generator.cpp
		test_join.cpp
		test_mutex.cpp
		test_shared_mutex.cpp		
		test_stream.cpp
		test_task.cpp
		test_thread_pool.cpp
		test_event.cpp
		test_sleep.cpp
		test_semaphore.cpp
		testing/test_interleaver.cpp
		helper_schedulers.hpp
		monitor_task.hpp
		monitor_allocator.hpp
)


find_package(Catch2 3 REQUIRED)
target_link_libraries(test Catch2::Catch2)
target_link_libraries(test asyncpp)