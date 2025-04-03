#ifndef UTIL_SOCKET_H
#define UTIL_SOCKET_H

#define BUFFER_SIZE 1024

int connect_to_socket(const char *socket_path);
int close_sockect(int socket_fd);

#endif // UTIL_SOCKET_H
