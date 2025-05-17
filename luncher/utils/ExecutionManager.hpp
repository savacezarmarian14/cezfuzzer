#pragma once

#include "ConfigurationManager.hpp"
#include <string>
#include <vector>
#include <optional>
#include <sys/types.h>

namespace utils {

class ExecutionManager {
public:
    ExecutionManager();
    explicit ExecutionManager(const ConfigurationManager& config);
    std::optional<pid_t> launchEntity(const EntityConfig& entity);

private:
    ConfigurationManager config_;
    std::vector<char*> buildArgv(const EntityConfig& entity);
};

}  // namespace utils
