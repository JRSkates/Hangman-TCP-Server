#ifndef _SERVER_H_
#define _SERVER_H_

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

// Server Configuration Constants
#define PORT 8080         // The port number the server listens on
// #define player_count 3 // Maximum number of players allowed in the game
#define MAX_GUESSES 8     // Maximum wrong guesses allowed per player

// Function Declarations
int create_server(int player_count);
void add_new_player(int server_fd, int *client_sockets, int *connections_pending_name_input, int *connections_players);
int reject_incoming_connections(int server_fd, fd_set *readfds);
void handle_client_name_input(int *client_sockets, char **player_names, int *name_received, int *connected_players, int *connections_pending_name_input);
void handle_ready_up(int server_fd, int *client_sockets, fd_set *readfds, char **player_names, int *connected_players);
void play_hangman(int server_fd, int *client_sockets, int *connected_players, char *goal_word, fd_set *readfds, char **player_names);
int is_word_guessed(int *player_progress, int word_length);
void format_and_send_leaderboard(int server_fd, int *client_sockets, int *connected_players, int *leaderboard, char * goal_word, fd_set *readfds, char **player_names);
void random_goal_word();

// Global pointer to dynamically allocated goal word
extern char *goal_word; 
extern int max_player_count;
extern int player_count;

#endif 