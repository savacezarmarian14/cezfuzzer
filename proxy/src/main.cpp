#include <iostream>
#include "ProxyBase.hpp"
#include <pthread.h>
#include <unistd.h>

void* flush_thread_func(void* arg) {
    printf("[DEBUG] Flusher thread started...\n");
    while (1) {
        fflush(stdout);
        fflush(stderr);
        usleep(500000); // 0.5 secunde = 500.000 microsecunde
    }
    return NULL;
}

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

    pthread_t flush_thread;
    int ret = pthread_create(&flush_thread, NULL, flush_thread_func, NULL);
    if (ret != 0) {
        perror("[ERROR] pthread_create (flush thread)");
    } else {
        pthread_detach(flush_thread);
    }

    while(1); // FOR THE MOMENT [TODO]

    return 0;
}
