cmake_minimum_required(VERSION 3.25)

project(async++)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	if (ENABLE_LLVM_COV)
		add_compile_options("-fprofile-instr-generate" "-fcoverage-mapping" "-mllvm" "-enable-name-compression=false")
		add_link_options("-fprofile-instr-generate" "-fcoverage-mapping")
	endif()
	if (ENABLE_LLVM_ADDRESS_SANITIZER)
		add_compile_options("-fsanitize=address")
		add_link_options("-fsanitize=address")
	endif()
	if (ENABLE_LLVM_MEMORY_SANITIZER)
		add_compile_options("-fsanitize=memory")
		add_link_options("-fsanitize=memory")
	endif()
endif()

add_subdirectory(include/async++)
add_subdirectory(src)
add_subdirectory(test)