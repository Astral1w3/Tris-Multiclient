#include "protocol_commands.h"

#include <string.h>

/**
 * @brief Effettua il parsing di un messaggio testuale separando comando e argomenti.
 *        Ignora spazi iniziali e divide la stringa al primo spazio.
 * @param message   Stringa del messaggio ricevuto dal client.
 * @param cmd       Buffer di output per il comando (max 63 char).
 * @param args      Buffer di output per gli argomenti.
 * @param args_size Dimensione del buffer args.
 * @return 0 in caso di successo, -1 se il messaggio è invalido o vuoto.
 */
int parse_command(const char* message, char* cmd, char* args, size_t args_size) {
    char  msg_copy[1024];
    char* space = NULL;

    if (!message || !cmd || !args || args_size == 0) return -1;

    cmd[0]  = '\0';
    args[0] = '\0';

    while (*message == ' ') ++message;

    strncpy(msg_copy, message, sizeof(msg_copy) - 1);
    msg_copy[sizeof(msg_copy) - 1] = '\0';
    if (msg_copy[0] == '\0') return -1;

    space = strchr(msg_copy, ' ');
    if (!space) {
        strncpy(cmd, msg_copy, 63);
        cmd[63] = '\0';
        return 0;
    }

    *space = '\0';
    ++space;
    while (*space == ' ') ++space;

    strncpy(cmd, msg_copy, 63);
    cmd[63] = '\0';
    strncpy(args, space, args_size - 1);
    args[args_size - 1] = '\0';
    return 0;
}
