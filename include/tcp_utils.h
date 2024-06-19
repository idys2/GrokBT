#ifndef TCP_UTILS_H
#define TCP_UTILS_H

#include <arpa/inet.h>

int sendall(int s, const char *buf, uint32_t *len);
int sendall(int s, uint8_t *buf, uint32_t *len);

#endif