#ifndef PROTOCOL_LOBBY_H
#define PROTOCOL_LOBBY_H

#include "game.h"

/*
 * protocol_lobby — gestione lobby: lista partite, creazione, richiesta
 * join e risposta join. Contiene anche la broadcast dello stato lobby.
 */

void broadcast_games_update(void);

void handle_create_game(Player* player, const char* args);
void handle_list_games(Player* player);
void handle_join_game(Player* player, const char* args);
void handle_answer_join(Player* player, const char* args);

#endif // PROTOCOL_LOBBY_H
