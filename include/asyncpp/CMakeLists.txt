add_library(asyncpp-headers INTERFACE)


target_include_directories(asyncpp-headers INTERFACE "${CMAKE_CURRENT_LIST_DIR}/..")


target_sources(asyncpp-headers
	INTERFACE FILE_SET headers TYPE HEADERS FILES
		container/atomic_collection.hpp
		container/atomic_deque.hpp
		container/atomic_item.hpp
		container/atomic_stack.hpp
		memory/rc_ptr.hpp
		testing/interleaver.hpp
		testing/suspension_point.hpp
		threading/spinlock.hpp
		threading/cache.hpp
		concepts.hpp
		event.hpp
		generator.hpp
		join.hpp
		lock.hpp
		mutex.hpp
		promise.hpp
		scheduler.hpp
		semaphore.hpp
		shared_mutex.hpp
		sleep.hpp
		stream.hpp
		task.hpp
		thread_pool.hpp	
)


set_target_properties(asyncpp-headers PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)