add_executable(benchmark)

target_sources(benchmark
	PRIVATE
		main.cpp
		benchmark_task_spawn.cpp
		benchmark_thread_pool.cpp
		benchmark_atomic.cpp
)


find_package(Celero REQUIRED)
target_link_libraries(benchmark celero)
target_link_libraries(benchmark asyncpp)