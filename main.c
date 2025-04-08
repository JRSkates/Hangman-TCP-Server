#include <stdio.h>      // Standard input/output functions
#include <stdlib.h>     // Standard library functions (malloc, free, exit)
#include <string.h>     // String manipulation functions (memset, strlen)
#include <unistd.h>     // POSIX API functions (close, read, write)
#include <arpa/inet.h>  // Networking functions (socket, bind, listen, accept, inet_ntoa)
#include <sys/types.h>  // System data types (for socket operations)
#include <sys/socket.h> // Socket programming functions
#include <sys/select.h> // Multiplexing functions (select, FD_SET, etc.)
#include <ctype.h>
#include <time.h>

#include "server.h"

int main(void) {
    srand(time(NULL)); // Ensure randomness
    printf("Enter the maximum number of players allowed in the game: ");
    scanf("%d", &max_player_count);
    printf("Max players: %d\n", max_player_count);
    player_count = max_player_count;

    int *client_sockets = malloc(player_count * sizeof(int));
    char **player_names = malloc(player_count * sizeof(char *));
    int *name_received = malloc(player_count * sizeof(int));
    int *leaderboard = malloc(player_count * sizeof(int));

    if (!client_sockets || !player_names || !name_received || !leaderboard) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    memset(client_sockets, 0, player_count * sizeof(int));
    memset(name_received, 0, player_count * sizeof(int));
    memset(leaderboard, 0, player_count * sizeof(int));
    for (int i = 0; i < player_count; i++) {
        player_names[i] = NULL; // Initialize to NULL
    }

    int server_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    // Assign goal_word randomly from pool of words
    random_goal_word();
    printf("Goal Word: %s\n", goal_word);

    // Create the server socket and start listening
    server_fd = create_server(player_count);

    int connected_players = 0;  // Tracks players who have entered names and fully connected to the game
    int connections_pending_name_input = 0; // Tracks active sockets that haven't sent their name

    fd_set readfds;

    // Accept new players and handle their name input until all players have entered their names
    while (connected_players < player_count) {
        FD_ZERO(&readfds); // Clear the file descriptor set
        FD_SET(server_fd, &readfds); // Add server socket to the set
        int max_sd = server_fd;

        // Add client sockets to the read set
        for (int i = 0; i < player_count; i++) {
            int sd = client_sockets[i];
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        printf("Waiting for players...\n");
        printf("Connected players: %d\n\n", connected_players);

        // Wait for activity on any socket
        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        // Accept new players if space is available
        if (FD_ISSET(server_fd, &readfds)) {
            if (connections_pending_name_input < player_count) {
                add_new_player(server_fd, client_sockets, &connections_pending_name_input, &connected_players);
                printf("Spaces available: %d\n", player_count - connections_pending_name_input);
            } else {
                // Reject extra connections
                int new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len);
                if (new_socket < 0) {
                    perror("Accept failed");
                    exit(EXIT_FAILURE);
                }
        
                int reject_value = -1;
                send(new_socket, &reject_value, sizeof(reject_value), 0);
                close(new_socket);
                printf("Rejected connection from unknown client as server is full\n");
            }
        }

        // Handle player name input asynchronously
        handle_client_name_input(client_sockets, player_names, name_received, &connected_players, &connections_pending_name_input);
    }

    printf("Connected players DEBUG: %d\n", connected_players);
    // Send ready-up message to all players
    char ready_message[] = "All players have entered their usernames. Ready up by entering 'r'\n";
    for (int i = 0; i < player_count; i++) {
        send(client_sockets[i], ready_message, strlen(ready_message), 0);
    }

    // Wait for all players to send 'r'
    handle_ready_up(server_fd, client_sockets, &readfds, player_names, &connected_players);

    // Main Game loop
    play_hangman(server_fd, client_sockets, &connected_players, goal_word, &readfds, player_names);


    char leaderboard_message[] = "All Players have finished! Generating leaderboard...\n";
    for (int i = 0; i < player_count; i++) {
        send(client_sockets[i], leaderboard_message, strlen(ready_message), 0);
    }

    // Receive final scores in formatted string: "Username:Score"
    format_and_send_leaderboard(server_fd, client_sockets, &connected_players, leaderboard, goal_word, &readfds, player_names);

    // Close all client sockets and free allocated memory
    for (int i = 0; i < player_count; i++) {
        if (client_sockets[i] > 0) {
            close(client_sockets[i]);
            free(player_names[i]);
            player_names[i] = NULL;
        }
    }

    // Free allocated memory before exiting
    free(client_sockets);
    free(player_names);
    free(name_received);
    free(leaderboard);
    free(goal_word);
    goal_word = NULL;

    close(server_fd); // Close the server socket
    return 0;
}