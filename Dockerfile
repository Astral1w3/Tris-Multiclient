# ─── Stage 1: build ──────────────────────────────────────────────────────────
FROM gcc:14-bookworm AS builder

WORKDIR /app

# Copia tutto il sorgente (il .dockerignore esclude obj/, binari, ecc.)
COPY . .

# Compila senza simboli di debug per ridurre la dimensione dei binari
RUN make clean && make CFLAGS="-Wall -Wextra -std=c11 -pthread"

# ─── Stage 2: runtime ────────────────────────────────────────────────────────
FROM debian:bookworm-slim AS runtime

WORKDIR /app

# Utente non-root con UID fisso (best practice sicurezza)
RUN useradd -m -u 10001 tris

# Copia solo i binari necessari dallo stage builder
COPY --from=builder /app/server /app/server
COPY --from=builder /app/client /app/client

USER tris

EXPOSE 8888

ENTRYPOINT ["./server"]
CMD ["8888"]
