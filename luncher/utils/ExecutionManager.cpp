#include "ExecutionManager.hpp"
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <sys/stat.h>

namespace utils {

ExecutionManager::ExecutionManager() = default;

ExecutionManager::ExecutionManager(const ConfigurationManager& config) : config_(config) {}

std::optional<pid_t> ExecutionManager::launchEntity(const EntityConfig& entity, int index)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("[ERROR] fork()");
        return std::nullopt;
    }

    if (pid == 0) {
        // Child process
        std::string log_path = "/tmp/logs/" + entity.name + "_" + std::to_string(index) + ".log";
        mkdir("/tmp/logs", 0755); // creează folderul dacă nu există

        int fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("[ERROR] open(log file)");
            exit(EXIT_FAILURE);
        }

        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);

        std::vector<char*> argv = buildArgv(entity);

        printf("[DEBUG] Launching entity \"%s\" with arguments:\n", entity.name.c_str());
        for (size_t i = 0; i < argv.size() - 1; ++i) { // exclude nullptr
            printf("  argv[%zu]: %s\n", i, argv[i]);
        }

        std::vector<char*> new_argv;
        new_argv.push_back(strdup("stdbuf"));
        new_argv.push_back(strdup("-o0")); // stdout non-buffered
        new_argv.push_back(strdup("-e0")); // stderr non-buffered
        for (auto ptr : argv) {
            new_argv.push_back(ptr);
        }
        new_argv.push_back(nullptr);
        execvp(new_argv[0], new_argv.data());

        // Dacă exec eșuează
        perror("[ERROR] execvp()");
        exit(EXIT_FAILURE);
    }

    std::vector<char*> argv = buildArgv(entity);

    printf("[DEBUG] Launching entity \"%s\" with arguments:\n", entity.name.c_str());
    for (size_t i = 0; i < argv.size() - 1; ++i) { // exclude nullptr
        printf("  argv[%zu]: %s\n", i, argv[i]);
    }

    // Părinte
    std::cout << "[INFO] Launched " << entity.name << " (PID " << pid << ")\n";
    return pid;
}

std::vector<char*> ExecutionManager::buildArgv(const EntityConfig& entity)
{
    std::vector<char*> argv;

    if (!entity.exec_with.empty()) {
        argv.push_back(strdup(entity.exec_with.c_str()));
    }

    std::string full_path = "/app/" + entity.binary_path;
    argv.push_back(strdup(full_path.c_str()));

    for (const auto& arg : entity.args) {
        argv.push_back(strdup(arg.c_str()));
    }

    if (entity.role == "client" && entity.connect_to.has_value()) {
        argv.push_back(strdup(entity.connect_to->ip.c_str()));
        argv.push_back(strdup(std::to_string(entity.connect_to->port).c_str()));
    }

    argv.push_back(nullptr); // terminator

    return argv;
}

} // namespace utils
