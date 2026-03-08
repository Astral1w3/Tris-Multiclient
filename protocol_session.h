#ifndef PROTOCOL_SESSION_H
#define PROTOCOL_SESSION_H

#include "game.h"

/*
 * protocol_session — gestione ciclo di vita del giocatore:
 * set nickname, quit e cleanup alla disconnessione.
 */

void handle_set_nickname(Player* player, const char* args);
void handle_quit(Player* player);

#endif // PROTOCOL_SESSION_H
