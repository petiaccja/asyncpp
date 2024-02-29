#pragma once


#include <cstddef>
#include <new>


namespace asyncpp {

#ifdef __cpp_lib_hardware_interference_size
inline constexpr size_t avoid_false_sharing = std::hardware_destructive_interference_size;
inline constexpr size_t promote_true_sharing = std::hardware_constructive_interference_size;
#else
inline constexpr size_t avoid_false_sharing = 64;
inline constexpr size_t promote_true_sharing = 64;
#endif

} // namespace asyncpp