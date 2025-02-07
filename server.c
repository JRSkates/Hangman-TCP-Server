#include <stdio.h>      // Standard input/output functions
#include <stdlib.h>     // Standard library functions (malloc, free, exit)
#include <string.h>     // String manipulation functions (memset, strlen)
#include <unistd.h>     // POSIX API functions (close, read, write)
#include <arpa/inet.h>  // Networking functions (socket, bind, listen, accept, inet_ntoa)
#include <sys/types.h>  // System data types (for socket operations)
#include <sys/socket.h> // Socket programming functions
#include <sys/select.h> // Multiplexing functions (select, FD_SET, etc.)
#include <ctype.h>

// Server Configuration Constants
#define PORT 8080         // The port number the server listens on
#define PLAYER_COUNT 4  // Maximum number of players allowed in the game
#define MAX_GUESSES 6     // Maximum wrong guesses allowed per player

// Function Declarations
int create_server(int player_count);
void add_new_player(int server_fd, int *client_sockets, char **player_names, int *name_received, int *connections_pending_name_input);
void handle_client_name_input(int *client_sockets, char **player_names, int *name_received, int *connected_players, int *connections_pending_name_input);
void handle_ready_up(int *client_sockets, fd_set *readfds, char **player_names, int *connected_players);
void play_hangman(int *client_sockets, int connected_players, char *goal_word, fd_set *readfds, char **player_names);
int is_word_guessed(int *player_progress, int word_length);

// Global word for all clients to guess
char goal_word[] = "HELLO"; // Example word

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
    int connections_pending_name_input = 0; // Tracks active sockets that haven't sent their name

    fd_set readfds;

    // Accept new players and handle their name input until all players have entered their names
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
            if (connections_pending_name_input < PLAYER_COUNT) {
                add_new_player(server_fd, client_sockets, player_names, name_received, &connections_pending_name_input);
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
        handle_client_name_input(client_sockets, player_names, name_received, &connected_players, &connections_pending_name_input);
    }

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



    // Main Game loop
    play_hangman(client_sockets, connected_players, goal_word, &readfds, player_names);



    /* *** TO DO ***
    Receive final scores from all clients

    Format the final scores

    Send full leaderboard to all clients

    Program ends
    */




    //==================================================================================================================================

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
void add_new_player(int server_fd, int *client_sockets, char **player_names, int *name_received, int *connections_pending_name_input) {
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    int new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
    if (new_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    printf("New connection, socket fd: %d, ip: %s, port: %d\n",
           new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

    // send(new_socket, "Welcome to the game! Please enter your name:\n",
    //      strlen("Welcome to the game! Please enter your name:\n"), 0);

    // Store the client socket
    for (int i = 0; i < PLAYER_COUNT; i++) {
        if (client_sockets[i] == 0) { // Find empty slot
            client_sockets[i] = new_socket;
            name_received[i] = 0;
            (*connections_pending_name_input)++; // Increment immediately when a socket is accepted
            break;
        }
    }
}

// Handle client input asynchronously (players can enter names independently)
void handle_client_name_input(int *client_sockets, char **player_names, int *name_received, int *connected_players, int *connections_pending_name_input) {
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
                (*connections_pending_name_input)--;  // Allow a new player to join
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
                        printf("Player %d - %s is ready!\n", i + 1, player_names[i]);
                        ready_players++;
                    }
                } else if (valread == 0) {  // Player disconnected before readying up
                    printf("Player %d (Socket %d) disconnected before readying up.\n", 
                        i + 1, sd);
                    printf("Player numbers above Player %d will move down (Player %d is now Player %d etc)\n",
                        i + 1, i + 2, i + 1);
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


void play_hangman(int *client_sockets, int connected_players, char *goal_word, fd_set *readfds, char **player_names) {
    int word_length = strlen(goal_word);
    int guesses_left[connected_players]; // Stores remaining guesses for each player (associated by index position)
    int server_arr[connected_players][word_length]; // Nested tracking arrays for each clients progress when guessing the word
    int game_finished[connected_players]; // Tracks whether a player has finished

    // Send the length of the goal word to all clients
    for (int i = 0; i < connected_players; i++){
        send(client_sockets[i], &word_length, sizeof(word_length), 0);
        printf("Word length: %d sent to Player: %d\n", word_length, i + 1);
    }

    // Initialize guess tracking arrays and remaining guesses for each player
    for (int i = 0; i < connected_players; i++) {
        guesses_left[i] = MAX_GUESSES; // Start each player with max guesses
        memset(server_arr[i], 0, word_length * sizeof(int)); // Initalize server guess tracking arrays
        game_finished[i] = 0; // 0 means player has NOT finished
    }

    printf("Game started!\n");

    char guess;

    while (1) { // Keep looping until all players have finished
        FD_ZERO(readfds);
        int max_sd = 0;
        int active_players = 0;

        for (int i = 0; i < connected_players; i++) {  // Loop only through active players
            if (client_sockets[i] > 0 && !game_finished[i]) {
                FD_SET(client_sockets[i], readfds);
                if (client_sockets[i] > max_sd) {
                    max_sd = client_sockets[i];
                }
                active_players++;
            }
        }

        if (active_players == 0) {
            printf("All players have finished the game. Exiting...\n");
            break;
        }

        if (select(max_sd + 1, readfds, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            exit(EXIT_FAILURE);
        }

        // Process guesses for each player
        for (int i = 0; i < connected_players; i++) {
            int sd = client_sockets[i];

            if (game_finished[i]) {
                continue;
            }

            if (FD_ISSET(sd, readfds)) {
                memset(&guess, 0, sizeof(guess));
                int valread = recv(sd, &guess, sizeof(guess), 0);

                // Handle player disconnections
                if (valread == 0) {
                    printf("Player %d (Socket %d) disconnected during the game.\n", 
                        i + 1, sd);
                    printf("Player numbers above Player %d will move down (Player %d is now Player %d etc)\n",
                        i + 1, i + 2, i + 1);
                    close(sd);
                    client_sockets[i] = 0;

                    // Shift all remaining players down
                    for (int j = i; j < connected_players - 1; j++) {
                        client_sockets[j] = client_sockets[j + 1];
                        player_names[j] = player_names[j + 1];
                        guesses_left[j] = guesses_left[j + 1];
                        game_finished[j] = game_finished[j + 1];

                        // Copy nested server_arr state
                        memcpy(server_arr[j], server_arr[j + 1], word_length * sizeof(int));
                    }

                    // Clear the last slot
                    client_sockets[connected_players - 1] = 0;
                    player_names[connected_players - 1] = NULL;
                    guesses_left[connected_players - 1] = 0;
                    game_finished[connected_players - 1] = 1; // Mark as finished

                    memset(server_arr[connected_players - 1], 0, word_length * sizeof(int));

                    // Reduce the player count
                    connected_players--;

                    // Reduce active players count for the loop condition
                    active_players--;

                    // Adjust loop counter since we shifted elements
                    i--;
                    continue;
                } 
                
                // Handle player guess
                else if (valread > 0) { 
                    guess = toupper(guess); // Convert input to upper case

                    // Ignore newline and carriage return characters
                    if (guess == '\n' || guess == '\r') {
                        continue; // Skip this iteration and wait for a real input
                    }
                
                    // Ensure it's a valid alphabetical letter (A-Z only)
                    if (guess < 'A' || guess > 'Z') {
                        printf("Invalid input received from Player %d: %c (ASCII: %d)\n", i + 1, guess, guess);
                        continue; // Ignore anything that isn't a valid letter
                    }

                    printf("Player %d: guessed %c\n", i + 1, guess);

                    int boolean_arr[word_length]; // Temp array to send back to client with guess results
                    memset(boolean_arr, 0, word_length * sizeof(int));

                    int correct_guess = 0;

                    // Check if the guessed letter is in the goal word
                    for (int j = 0; j < word_length; j++) {
                        if (goal_word[j] == guess) {
                            boolean_arr[j] = 1;
                            server_arr[i][j] = 1;
                            correct_guess = 1;
                        }
                    }

                    // If guess if incorrect, lose a life
                    if (!correct_guess) {
                        guesses_left[i]--; // Decrement remaining guesses
                        printf("Player %d: incorrect guess. Remaining guesses: %d\n", i + 1, guesses_left[i]);
                    } else {
                        printf("Player %d: correct guess.\n", i + 1);
                    }

                    // Send the updated boolean array with guessed letters to all clients
                    send(client_sockets[i], boolean_arr, word_length * sizeof(int), 0);

                    // Check if the player has finished (either guessed the word in full, or out of guesses)
                    if (is_word_guessed(server_arr[i], word_length)) {
                        printf("Player %d: has guessed the word!\n", i + 1);
                        game_finished[i] = 1;
                    }

                    if (guesses_left[i] == 0) {
                        printf("Player %d is out of guesses\n", i + 1);
                        game_finished[i] = 1;
                    }
                }
            }
        }
    }
}

int is_word_guessed(int *player_progress, int word_length) {
    for (int i = 0; i < word_length; i++) {
        if (player_progress[i] == 0) {  // If any letter is still missing, return false
            return 0;
        }
    }
    return 1;  // All letters have been guessed
}





