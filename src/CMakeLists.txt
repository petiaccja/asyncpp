add_library(async++)

target_sources(async++
	PRIVATE
		scheduler.cpp
)

target_link_libraries(async++ async++-headers)