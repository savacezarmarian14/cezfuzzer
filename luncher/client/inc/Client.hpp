#ifndef __CLIENT_HPP__
#define __CLIENT_HPP__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/select.h>
#include <vector>
#include "ConfigurationManager.hpp"
#include "ExecutionManager.hpp"
#include "Messages.h"
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <optional>

#define MAX_MSG_SIZE 4096

/* Functions */
int init_client();
int start_client();

ssize_t send_message(int sockfd, const void* buffer, size_t len);
ssize_t recv_message(int sockfd, void* buffer);

/* Variables */
char                             IP[64];
utils::ConfigurationManager      cm;
utils::ExecutionManager          em;
std::vector<utils::EntityConfig> entities;
std::vector<pid_t>               processList;

#endif