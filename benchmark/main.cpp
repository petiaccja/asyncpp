#include <iostream>

#include <celero/Celero.h>


int main(int argc, char* argv[]) {
    try {
        celero::Run(argc, argv);
    }
    catch (std::exception& ex) {
        std::cerr << "benchmarks exited with error: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}