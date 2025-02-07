#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define PLAYER_COUNT 2  // Max number of players allowed
#define MAX_GUESSES 6

int create_server(int player_count);
void add_new_player(int server_fd, int *client_sockets, char **player_names, int *name_received, int *active_connections);
void handle_client_input(int *client_sockets, char **player_names, int *name_received, int *connected_players, int *active_connections);
void handle_ready_up(int *client_sockets, fd_set *readfds, char **player_names, int *connected_players);

int main(void) {
    int client_sockets[PLAYER_COUNT] = {0}; // Stores active client sockets
    char *player_names[PLAYER_COUNT] = {0}; // Stores player names
    int name_received[PLAYER_COUNT] = {0};  // Tracks if a player has entered their name
    int server_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    server_fd = create_server(PLAYER_COUNT);
    printf("Waiting for players...\n");

    int connected_players = 0;    // Tracks players who have entered names
    int active_connections = 0;   // Tracks active sockets (clients that connected)

    fd_set readfds;

    while (connected_players < PLAYER_COUNT) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        // Add client sockets to read set
        for (int i = 0; i < PLAYER_COUNT; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        // Accept new players only if active_connections < PLAYER_COUNT
        if (FD_ISSET(server_fd, &readfds)) {
            if (active_connections < PLAYER_COUNT) {
                add_new_player(server_fd, client_sockets, player_names, name_received, &active_connections);
            } else {
                // Reject extra connections
                int reject_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
                if (reject_socket > 0) {
                    char *message = "Server is full. Try again later.\n";
                    send(reject_socket, message, strlen(message), 0);
                    close(reject_socket);
                }
            }
        }

        // Handle player name input asynchronously
        handle_client_input(client_sockets, player_names, name_received, &connected_players, &active_connections);
    }

    // Send the ready-up message to all players
    char ready_message[1024] = "All players have entered their names. Ready up by entering 'r'\n";
    for (int i = 0; i < PLAYER_COUNT; i++) {
        send(client_sockets[i], ready_message, strlen(ready_message), 0);
    }

    // Wait for all players to send 'r'
    handle_ready_up(client_sockets, &readfds, player_names, &connected_players);


    // Print players and sockets for debugging
    for (int i = 0; i < PLAYER_COUNT; i++) {
        printf("Player %d: Socket %d, Name: %s\n", i, client_sockets[i], player_names[i]);
    }

    // Close all client sockets and free memory
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
            free(player_names[i]);
        }
    }

    close(server_fd);
    return 0;
}

// Accept a new player and store their socket
void add_new_player(int server_fd, int *client_sockets, char **player_names, int *name_received, int *active_connections) {
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
    if (new_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("New connection, socket fd: %d, ip: %s, port: %d\n",
           new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

    send(new_socket, "Welcome to the game! Please enter your name:\n",
         strlen("Welcome to the game! Please enter your name:\n"), 0);

    // Store the client socket
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (client_sockets[i] == 0) { // Find empty slot
            client_sockets[i] = new_socket;
            name_received[i] = 0;
            (*active_connections)++; // Increment immediately when a socket is accepted
            break;
        }
    }
}

// Handle client input asynchronously (players can enter names independently)
void handle_client_input(int *client_sockets, char **player_names, int *name_received, int *connected_players, int *active_connections) {
    char name_buffer[50];

    for (int i = 0; i < PLAYER_COUNT; i++) {
        int sd = client_sockets[i];

        if (sd > 0) {
            memset(name_buffer, 0, sizeof(name_buffer));
            
            // Use recv() with MSG_DONTWAIT to avoid blocking
            int valread = recv(sd, name_buffer, sizeof(name_buffer), MSG_DONTWAIT);
            
            if (valread == 0) {  // Client has disconnected
                printf("Player %d (Socket %d) disconnected.\n", i, sd);
                close(sd);
                client_sockets[i] = 0;  // Free up the slot
                free(player_names[i]);
                player_names[i] = NULL;
                name_received[i] = 0;
                (*active_connections)--;  // Allow a new player to join
            } else if (valread > 0 && !name_received[i]) {
                name_buffer[strcspn(name_buffer, "\n")] = 0;  // Remove newline
                player_names[i] = strdup(name_buffer);
                printf("Player %d registered as: %s\n", i, player_names[i]);
                name_received[i] = 1;
                (*connected_players)++;
            }
        }
    }
}


int create_server(int player_count) {
    int server_fd;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind address and port to socket
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for connections
    if (listen(server_fd, player_count) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    return server_fd;
}

void handle_ready_up(int *client_sockets, fd_set *readfds, char **player_names, int *connected_players) {
    int ready_players = 0;
    char buffer[10];

    printf("Waiting for all players to ready up...\n");

    while (ready_players < PLAYER_COUNT) {
        FD_ZERO(readfds);

        int max_sd = 0;
        for (int i = 0; i < PLAYER_COUNT; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], readfds);
                if (client_sockets[i] > max_sd) {
                    max_sd = client_sockets[i];
                }
            }
        }

        if (select(max_sd + 1, readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < PLAYER_COUNT; i++) {
            int sd = client_sockets[i];

            if (FD_ISSET(sd, readfds)) {
                memset(buffer, 0, sizeof(buffer));
                int valread = recv(sd, buffer, sizeof(buffer), 0);

                if (valread > 0) {
                    if (buffer[0] == 'r') {
                        printf("Player %d is ready!\n", i);
                        ready_players++;
                    }
                } else if (valread == 0) {  // Client disconnected before readying up
                    printf("Player %d (Socket %d) disconnected before readying up.\n", i, sd);
                    close(sd);
                    client_sockets[i] = 0;  // Free the slot

                    // Free the player's name and reset their state
                    free(player_names[i]);
                    player_names[i] = NULL;
                    (*connected_players)--;  // Reduce total connected players

                    // Reduce `ready_players` count if they had already sent 'r'
                    if (buffer[0] == 'r') {
                        ready_players--;
                    }
                }
            }
        }
    }

    printf("All players are ready! Starting the game...\n");
}


