#include <iostream>
#include "ProxyBase.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config.yaml path>" << std::endl;
        return 1;
    }

    const char* config_path = argv[1];

    try {
        ProxyBase proxy(config_path);
        // proxy.run(); // în viitor, dacă adaugi o metodă de rulare completă
    } catch (const std::exception& ex) {
        std::cerr << "Eroare la inițializarea ProxyBase: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
