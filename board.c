#include "board.h"
#include <string.h>
#include <pthread.h>

/**
 * @brief Controlla se c'è un vincitore sulla board della partita (thread-safe).
 *        Verifica righe, colonne, diagonali e patta.
 * @param g Puntatore alla partita da verificare.
 * @return 'X' o 'O' se c'è un vincitore, 'D' per patta, ' ' se in corso.
 */
char game_check_winner(Game* g) {
    if (!g) {
        return ' ';
    }

    char b[3][3];
    pthread_mutex_lock(&g->mutex);
    memcpy(b, g->board, sizeof(g->board));
    pthread_mutex_unlock(&g->mutex);

    // Controllo righe
    for (int i = 0; i < 3; ++i) {
        if (b[i][0] != ' ' && b[i][0] == b[i][1] && b[i][1] == b[i][2]) {
            return b[i][0];
        }
    }

    // Controllo colonne
    for (int i = 0; i < 3; ++i) {
        if (b[0][i] != ' ' && b[0][i] == b[1][i] && b[1][i] == b[2][i]) {
            return b[0][i];
        }
    }

    // Diagonale principale
    if (b[0][0] != ' ' && b[0][0] == b[1][1] && b[1][1] == b[2][2]) {
        return b[0][0];
    }

    // Diagonale secondaria
    if (b[0][2] != ' ' && b[0][2] == b[1][1] && b[1][1] == b[2][0]) {
        return b[0][2];
    }

    // Controllo patta (tabellone pieno)
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (b[i][j] == ' ') {
                return ' '; // Partita non ancora conclusa
            }
        }
    }

    return 'D'; // Patta
}

/**
 * @brief Converte un GameResult nel corrispondente stringa leggibile.
 * @param r Risultato della partita (RESULT_WIN, RESULT_LOSS, RESULT_DRAW).
 * @return Stringa "WIN", "LOSS", "DRAW" o "NONE".
 */
const char* game_result_to_string(GameResult r) {
    if (r == RESULT_WIN)  return "WIN";
    if (r == RESULT_LOSS) return "LOSS";
    if (r == RESULT_DRAW) return "DRAW";
    return "NONE";
}
