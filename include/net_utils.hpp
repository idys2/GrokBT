#ifndef NET_UTILS_HPP
#define NET_UTILS_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <assert.h>
#include <string>

int sendall(int s, const char *buf, uint32_t *len);
int sendall(int s, uint8_t *buf, uint32_t *len);
void *get_in_addr(struct sockaddr *sa);
int get_listener_socket(int port, int listen_queue_size);

sockaddr_in get_self_sockaddr(int port);
#endif