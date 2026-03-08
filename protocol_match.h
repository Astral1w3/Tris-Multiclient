#ifndef PROTOCOL_MATCH_H
#define PROTOCOL_MATCH_H

#include "game.h"

/*
 * protocol_match — logica della partita attiva:
 * mosse, abbandono, terminazione e helper condivisi di stato player.
 */

/* Helper condivisi usati anche da protocol_lobby e protocol_session. */
Game* find_player_active_game(Player* player);
void  set_player_idle(Player* player);

/* API pubblica partita. */
void terminate_game(Game* game, GameResult owner_result, GameResult guest_result);
void handle_move(Player* player, const char* args);
void handle_leave_game(Player* player);

#endif // PROTOCOL_MATCH_H
