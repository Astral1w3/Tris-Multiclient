#ifndef PROTOCOL_IO_H
#define PROTOCOL_IO_H

#include "game.h"
#include "board.h"

/*
 * protocol_io — formattazione e invio di tutti i messaggi verso i client.
 * Non contiene logica di gioco.
 */

int  send_message(int socket_fd, const char* format, ...);
int  send_error(int socket_fd, const char* error_msg);
int  send_ok(int socket_fd);
int  send_welcome(int socket_fd);
int  send_games_update(int socket_fd);
int  send_join_request(int socket_fd, int game_id, const char* requester_nickname);
int  send_start_game(int socket_fd, char symbol, int game_id);
int  send_board_update(int socket_fd, Game* game);
int  send_game_over(int socket_fd, GameResult result, Game* game);
int  send_turn(int socket_fd, char symbol);
int  send_waiting(int socket_fd, const char* message);

#endif // PROTOCOL_IO_H
