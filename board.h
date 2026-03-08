#ifndef BOARD_H
#define BOARD_H

#include "game.h"

/*
 * game_check_winner - controlla l'esito del tabellone.
 *
 * Ritorna:
 *   'X' o 'O'  se uno dei due ha vinto
 *   'D'        se la partita è patta (tabellone pieno, nessun vincitore)
 *   ' '        se la partita non è ancora conclusa
 */
char game_check_winner(Game* game);

/*
 * game_result_to_string - converte un GameResult nel corrispondente
 * letterale stringa: "WIN", "LOSS", "DRAW" o "NONE".
 */
const char* game_result_to_string(GameResult result);

#endif // BOARD_H
