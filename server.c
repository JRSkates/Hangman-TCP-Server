#include <stdio.h>      // Standard input/output functions
#include <stdlib.h>     // Standard library functions (malloc, free, exit)
#include <string.h>     // String manipulation functions (memset, strlen)
#include <unistd.h>     // POSIX API functions (close, read, write)
#include <arpa/inet.h>  // Networking functions (socket, bind, listen, accept, inet_ntoa)
#include <sys/types.h>  // System data types (for socket operations)
#include <sys/socket.h> // Socket programming functions
#include <sys/select.h> // Multiplexing functions (select, FD_SET, etc.)

// Server Configuration Constants
#define PORT 8080         // The port number the server listens on
#define PLAYER_COUNT 4   // Maximum number of players allowed in the game
#define MAX_GUESSES 6     // Maximum wrong guesses allowed per player

// Function Declarations
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

    // Create the server socket and start listening
    server_fd = create_server(PLAYER_COUNT);
    printf("Waiting for players...\n");

    int connected_players = 0;  // Tracks players who have entered names and fully connected to the game
    int active_connections = 0; // Tracks active sockets (clients that connected)

    fd_set readfds;

    while (connected_players < PLAYER_COUNT) {
        FD_ZERO(&readfds); // Clear the file descriptor set
        FD_SET(server_fd, &readfds); // Add server socket to the set
        int max_sd = server_fd;

        // Add client sockets to the read set
        for (int i = 0; i < PLAYER_COUNT; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        // Wait for activity on any socket
        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        // Accept new players if space is available
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

    // Print player list for debugging
    // for (int i = 0; i < connected_players; i++) {
    //     printf("Player %d: Socket %d, Name: %s\n", i + 1, client_sockets[i], player_names[i]);
    // }

    // Send ready-up message to all players
    char ready_message[] = "All players have entered their names. Ready up by entering 'r'\n";
    for (int i = 0; i < PLAYER_COUNT; i++) {
        send(client_sockets[i], ready_message, strlen(ready_message), 0);
    }

    // Wait for all players to send 'r'
    handle_ready_up(client_sockets, &readfds, player_names, &connected_players);

    // Print final player list for debugging
    // for (int i = 0; i < connected_players; i++) {
    //     printf("Player %d: Socket %d, Name: %s\n", i + 1, client_sockets[i], player_names[i]);
    // }






    

    // Close all client sockets and free allocated memory
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
            free(player_names[i]);
        }
    }

    close(server_fd); // Close the server socket
    return 0;
}

// Function to create and configure the server socket
int create_server(int player_count) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address structure
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind socket to address and port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Set up listening queue
    if (listen(server_fd, player_count) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    return server_fd;
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
                printf("Player %d (Socket %d) disconnected.\n", i + 1, sd);
                close(sd);
                client_sockets[i] = 0;  // Free up the slot
                free(player_names[i]);
                player_names[i] = NULL;
                name_received[i] = 0;
                (*active_connections)--;  // Allow a new player to join
            } else if (valread > 0 && !name_received[i]) {
                name_buffer[strcspn(name_buffer, "\n")] = 0;  // Remove newline
                player_names[i] = strdup(name_buffer);
                printf("Player %d registered as: %s\n", i + 1, player_names[i]);
                name_received[i] = 1;
                (*connected_players)++;
            }
        }
    }
}

// Handle clients readying up, and adjusts if they disconnect during this process
void handle_ready_up(int *client_sockets, fd_set *readfds, char **player_names, int *connected_players) {
    int ready_players = 0; // Tracks how many players have sent 'r'
    char buffer[10];

    printf("Waiting for all players to ready up...\n");

    while (ready_players < *connected_players) {  // Dynamically wait for current players
        FD_ZERO(readfds);
        int max_sd = 0;

        for (int i = 0; i < *connected_players; i++) {  // Loop only through active players
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

        for (int i = 0; i < *connected_players; i++) {  // Iterate through active players
            int sd = client_sockets[i];

            if (FD_ISSET(sd, readfds)) {
                memset(buffer, 0, sizeof(buffer));
                int valread = recv(sd, buffer, sizeof(buffer), 0);

                if (valread > 0) {  // Player sent input
                    if (buffer[0] == 'r') {
                        printf("Player %d is ready!\n", i + 1);
                        ready_players++;
                    }
                } else if (valread == 0) {  // Player disconnected before readying up
                    printf("Player %d (Socket %d) disconnected before readying up.\n", i + 1, sd);
                    close(sd);
                    client_sockets[i] = 0;  // Free the slot

                    // Shift all players down to fill the gap
                    for (int j = i; j < *connected_players - 1; j++) {
                        client_sockets[j] = client_sockets[j + 1];
                        player_names[j] = player_names[j + 1];
                    }

                    // Clear the last slot
                    client_sockets[*connected_players - 1] = 0;
                    player_names[*connected_players - 1] = NULL;

                    // Reduce total connected players count
                    (*connected_players)--;

                    // Adjust ready_players count **ONLY IF** the disconnected player was already ready
                    if (ready_players > 0 && buffer[0] == 'r') {
                        ready_players--;
                    }

                    // Since a player left, we must update the loop bounds correctly
                    i--;
                }
            }
        }
    }

    printf("All players are ready! Starting the game...\n");
}






