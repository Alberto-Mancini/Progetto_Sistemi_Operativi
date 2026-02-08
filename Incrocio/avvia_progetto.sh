#!/bin/bash

# Compilazione
# -lrt e -pthread servono per shm e semafori
echo "Compilazione in corso..."
gcc main.c -o progetto_incrocio -lrt -pthread

if [ $? -eq 0 ]; then
    echo "Compilazione riuscita."
    echo "Avvio applicazione..."
    echo "NOTA: Per terminare usa 'kill -SIGTERM <PID_INCROCIO>' da un altro terminale."
    ./progetto_incrocio
else
    echo "Errore durante la compilazione."
fi