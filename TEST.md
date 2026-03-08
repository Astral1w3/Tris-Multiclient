# Guida al Testing

## Test Base

### 1. Avvio Server
```bash
./server 8888
```

### 2. Test Connessione Base
Apri un terminale e connetti un client:
```bash
./client 127.0.0.1 8888
```

Dovresti vedere:
```
Connesso al server 127.0.0.1:8888
Digita HELP per vedere i comandi disponibili

>>> WELCOME Benvenuto nel server Tris!
>>> MESSAGE Il tuo nickname è: Player_4. Usa SET_NICKNAME <nome> per cambiarlo.
```

### 3. Test Creazione Partita
Nel primo client:
```
SET_NICKNAME Alice
CREATE_GAME
LIST_GAMES
```

Dovresti vedere:
```
>>> MESSAGE Nickname impostato a: Alice
>>> OK
>>> MESSAGE Partita 1 creata. In attesa di sfidante...

=== PARTITE DISPONIBILI ===
ID: 1 - Owner: Alice
===========================
```

### 4. Test Join Partita
Apri un secondo terminale e connetti un altro client:
```bash
./client 127.0.0.1 8888
```

Nel secondo client:
```
SET_NICKNAME Bob
LIST_GAMES
JOIN_GAME 1
```

Nel primo client (Alice) dovresti vedere:
```
>>> RICHIESTA DI JOIN alla partita 1 da Bob
>>> Rispondi con: ANSWER_JOIN yes 1 Bob
```

Alice risponde:
```
ANSWER_JOIN yes 1 Bob
```

Entrambi i client dovrebbero vedere:
```
>>> PARTITA INIZIATA! Sei X (Partita ID: 1)  [per Alice]
>>> PARTITA INIZIATA! Sei O (Partita ID: 1)  [per Bob]

=== GRIGLIA ===
  0 1 2
0 . . .
1 . . .
2 . . .
Turno: X
==============
```

### 5. Test Gioco
Alice (X) fa la prima mossa:
```
MOVE 1 1
```

Entrambi vedono:
```
=== GRIGLIA ===
  0 1 2
0 . . .
1 . X .
2 . . .
Turno: O
==============
```

Bob (O) risponde:
```
MOVE 0 0
```

E così via fino alla fine della partita.

### 6. Test Fine Partita
Quando qualcuno vince o c'è un pareggio, entrambi vedono:
```
>>> PARTITA TERMINATA! Risultato: WIN  [per il vincitore]
>>> PARTITA TERMINATA! Risultato: LOSS [per il perdente]
>>> MESSAGE Vuoi fare una nuova partita? Usa REMATCH yes/no
```

### 7. Test Rematch
Il vincitore può rigiocare:
```
REMATCH yes
```

Se il vincitore era il guest (Bob), diventa il nuovo owner e Alice viene "cacciata".

## Test Edge Cases

### Test Disconnessione Owner
1. Alice crea una partita
2. Alice chiude il client (Ctrl+C o QUIT)
3. La partita viene automaticamente rimossa dalla lista

### Test Disconnessione Guest
1. Alice crea partita, Bob si unisce
2. Bob chiude il client durante il gioco
3. Alice riceve notifica e la partita torna in WAITING

### Test Mossa Non Valida
```
MOVE 5 5
```
Dovresti vedere:
```
>>> ERRORE: Mossa non valida
```

### Test Mossa Fuori Turno
Se non è il tuo turno:
```
>>> ERRORE: Non è il tuo turno
```

### Test Timeout
Il server ha un timeout di 5 minuti. Se un giocatore non muove per 5 minuti, l'altro vince automaticamente.

## Test Multi-Partita

1. Alice crea partita 1
2. Bob crea partita 2
3. Charlie si connette e vede entrambe le partite
4. Charlie può unirsi a una delle due

## Note

- Ogni giocatore può creare più partite, ma giocare solo una alla volta
- Le partite in stato WAITING sono visibili a tutti
- Solo i due giocatori di una partita vedono le mosse
