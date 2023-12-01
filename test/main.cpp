#define CATCH_CONFIG_RUNNER
#include <iostream>

#include <catch2/catch_session.hpp>


int main(int argc, char* argv[]) {
#if defined(__has_feature)
    #if __has_feature(address_sanitizer)
    std::cout << "Address sanitizer: ON" << std::endl;
    #endif
    #if __has_feature(memory_sanitizer)
    std::cout << "Memory sanitizer: ON" << std::endl;
    #endif
    #if __has_feature(thread_sanitizer)
    std::cout << "Thread sanitizer: ON" << std::endl;
    #endif
#endif

    int result = Catch::Session().run(argc, argv);

    return result;
}