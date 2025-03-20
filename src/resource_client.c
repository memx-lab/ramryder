#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define SERVER_SOCKET "/var/run/resource_manager.sock"
#define BUFFER_SIZE 1024

void send_command(const char *command) {
    int sockfd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SERVER_SOCKET, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Socket connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send(sockfd, command, strlen(command), 0);
    memset(buffer, 0, BUFFER_SIZE);
    recv(sockfd, buffer, BUFFER_SIZE, 0);
    
    printf("Server Response:\n");
    printf("%s\n", buffer);
    
    close(sockfd);
}

int main() {
    char command[256];

    printf("Enter command (query/hotplug/memory): ");
    if (scanf("%255s", command) <= 0) {
        fprintf(stderr, "Invalid input or input failure (EOF or empty input).\n");
        return -1;
    }

    send_command(command);
    return 0;
}
