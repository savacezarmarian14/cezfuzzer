#ifndef __SERVER_UTILS_HPP__
#define __SERVER_UITLS_HPP__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct client_info {
    int sockfd;
    char clientIP[16];
    unsigned int clientPORT;
    bool isCommander = false;
};

struct client_list {
    struct client_info *list = NULL;
    unsigned int no_clients = 0;
};

int addNewClient(int sockfd, char clientIP[16], unsigned int clientPORT,
                 struct client_list *list) 
{
    // Realocă spațiu pentru un client în plus
    struct client_info *new_list = (struct client_info *) realloc(list->list,
        (list->no_clients + 1) * sizeof(struct client_info));

    if (new_list == NULL) {
        perror("[ERROR] realloc client_list->list");
        return -1;
    }

    list->list = new_list;

    // Completează noul client
    struct client_info *ci = &list->list[list->no_clients];
    ci->sockfd = sockfd;
    strncpy(ci->clientIP, clientIP, 15);
    ci->clientIP[15] = '\0';  // siguranță
    ci->clientPORT = clientPORT;

    list->no_clients += 1;

    return 0;
}
int removeClient(int sockfd);






#endif