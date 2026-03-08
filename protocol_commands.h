#ifndef PROTOCOL_COMMANDS_H
#define PROTOCOL_COMMANDS_H

#include <stddef.h>

/*
 * protocol_commands — parsing dei messaggi grezzi in (comando, argomenti).
 *
 * Ritorna 0 in caso di successo, -1 se il messaggio è vuoto o invalido.
 * cmd deve avere almeno 64 byte, args almeno args_size byte.
 */
int parse_command(const char* message, char* cmd, char* args, size_t args_size);

#endif // PROTOCOL_COMMANDS_H
