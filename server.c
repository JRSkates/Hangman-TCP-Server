#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define PLAYER_COUNT 10
#define MAX_GUESSES 6

int create_server(int player_count);

int main(void) {
    int client_sockets[PLAYER_COUNT] = {0}; // Stores active client sockets
    char *player_names[PLAYER_COUNT] = {0}; // Stores player names
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    server_fd = create_server(PLAYER_COUNT);

    printf("Waiting for players...\n");

    int connected_players = 0;
    fd_set readfds;

    while(connected_players < PLAYER_COUNT) {
        FD_ZERO(&readfds); // Clear the set
        FD_SET(server_fd, &readfds); // Add the server socket to the set

        int max_sd = server_fd;

        // Add client sockets to the set and determine max socket descriptor
        for (int i = 0; i < PLAYER_COUNT; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }
    }
    
    return 0;
}

int create_server(int player_count) {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind address and port to socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connection up to the max player count 
    if (listen(server_fd, player_count) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    return server_fd;
}