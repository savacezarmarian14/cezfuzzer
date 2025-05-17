#ifndef __SERVER_HPP__
#define __SERVER_HPP__

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
#include "ConfigurationManager.hpp"
#include "ServerUtils.hpp"
#include "Messages.h"

#define MAX_MSG_SIZE 4096
#define CONFIG_FILE "/root/git-clones/cezfuzzer/config.yaml"
#define LISTEN_PORT 23927




/* Functions */
int init_server();
int start_server();

ssize_t send_message(int sockfd, const void *buffer, size_t len);
ssize_t recv_message(int sockfd, void *buffer);


void *listen_thread_func(void *arg);
void *handle_client_connection(void *arg);

/* Variables */
YAML::Node config;
pthread_t listen_thread;
utils::ConfigurationManager cm;
struct client_list client_list;


#endif