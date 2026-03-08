# Tris Server — Docker

Guida per compilare ed eseguire il server in container.

## Prerequisiti

- Docker installato e avviato.

## Build immagine

```bash
docker build -t tris-server:latest .
```

Il build usa un **multi-stage**:
- **Stage `builder`** — compila tutto con `gcc:14-bookworm`.
- **Stage `runtime`** — copia solo i binari (`server`, `client`) in `debian:bookworm-slim`, immagine finale leggera.

## Avvio server (porta default 8888)

```bash
docker run --rm -p 8888:8888 --name tris-server tris-server:latest
```

## Avvio server su porta personalizzata

Esempio su porta `9999`:

```bash
docker run --rm -p 9999:9999 --name tris-server tris-server:latest 9999
```

## Connessione da client locale

```bash
./client 127.0.0.1 8888
```

## Test completo in Docker (server + client nella stessa rete)

1. Crea una rete dedicata:

```bash
docker network create tris-net
```

2. Avvia il server in background:

```bash
docker run -d --name tris-server --network tris-net tris-server:latest
```

3. Avvia uno o più client interattivi:

```bash
docker run --rm -it --network tris-net --entrypoint ./client tris-server:latest tris-server 8888
```

4. Ferma il server:

```bash
docker stop tris-server && docker rm tris-server
```

## Note

- L'immagine runtime contiene solo `server` e `client` (nessun sorgente né toolchain).
- Il processo gira come utente non-root `tris` (UID 10001).
- La porta esposta di default è `8888`; cambiala con `-p <host>:<container>` e passando il numero come argomento.
