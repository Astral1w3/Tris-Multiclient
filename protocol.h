#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "game.h"
#include "board.h"

// ── Comandi Client → Server ──────────────────────────────────────────────────
#define CMD_CREATE_GAME     "CREATE_GAME"
#define CMD_LIST_GAMES      "LIST_GAMES"
#define CMD_JOIN_GAME       "JOIN_GAME"
#define CMD_ANSWER_JOIN     "ANSWER_JOIN"
#define CMD_MOVE            "MOVE"
#define CMD_LEAVE_GAME      "LEAVE_GAME"
#define CMD_QUIT            "QUIT"
#define CMD_SET_NICKNAME    "SET_NICKNAME"

// ── Risposte Server → Client ─────────────────────────────────────────────────
#define RSP_WELCOME         "WELCOME"
#define RSP_GAMES_UPDATE    "GAMES_UPDATE"
#define RSP_JOIN_REQ        "JOIN_REQ"
#define RSP_START_GAME      "START_GAME"
#define RSP_BOARD_UPD       "BOARD_UPD"
#define RSP_GAME_OVER       "GAME_OVER"
#define RSP_ERROR           "ERROR"
#define RSP_OK              "OK"
#define RSP_MESSAGE         "MESSAGE"
#define RSP_TURN            "TURN"
#define RSP_WAITING         "WAITING"

// ── Valori ANSWER_JOIN ───────────────────────────────────────────────────────
#define ANSWER_YES          "yes"
#define ANSWER_NO           "no"

// ── Sotto-moduli ─────────────────────────────────────────────────────────────
#include "protocol_io.h"
#include "protocol_commands.h"
#include "protocol_lobby.h"
#include "protocol_match.h"
#include "protocol_session.h"

#endif // PROTOCOL_H
