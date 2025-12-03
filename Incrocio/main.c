#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "incrocio.h"

#define NUM_SEMAFORI 4
#define FILE_INCROCIO "incrocio.txt"
#define FILE_AUTO "auto.txt"

struct Dati_condivisi{
    sem_t semafori_incrocio[4];
    sem_t ack_auto;
    int system_flag;
};

struct Dati_condivisi* Dati_condivisi;

// --- FUNZIONI --- //
void incrocio(struct Dati_condivisi* Dati_condivisi, int pipefd[]);
void garage(struct Dati_condivisi* Dati_condivisi, int pipefd[]);
int clear_memory();
int distruzione_semaforo(sem_t* semaforo);

int main(){
    
    int area_shm = shm_open("/shm_incrocio", O_CREAT | O_RDWR, 0666); //Aprima una zona di memoria condivisa per le fork
    if (area_shm == -1) {
        perror("Errore shm_open");
        exit(EXIT_FAILURE);
    }else{printf("[MAIN] Zona memoria condivisa creata correttamente.\n");}

    if (ftruncate(area_shm, sizeof(struct Dati_condivisi)) == -1) { //Crea la dimensione della memoria condivisa
        perror("Errore ftruncate");
        exit(EXIT_FAILURE);
    }else{printf("[MAIN] Dimensione memoria condivisa creata correttamente.\n");}

    Dati_condivisi = mmap(NULL, sizeof(struct Dati_condivisi), PROT_READ | PROT_WRITE, MAP_SHARED, area_shm, 0); //Mappa la memoria condivisa
    if (Dati_condivisi == MAP_FAILED) {
        perror("Errore mmap");
        exit(EXIT_FAILURE);
    }else{printf("[MAIN] Memoria condivisa creata correttamente.\n");}

    close(area_shm); //Chiudo il file descriptor della memoria condivisa

    //Inizializzazione semafori
    for(int i=0; i<NUM_SEMAFORI; i++){
        sem_init(&Dati_condivisi->semafori_incrocio[i],1,0); //Al centro c'è la condivisione del semaforo tra processi 
                                                            //mentre l'ultimo parametro è il valore iniziale del semaforo (rosso)
    }
    sem_init(&Dati_condivisi->ack_auto,1,0); //Semaforo di ack inizializzato a 0 (rosso)

    //Creazione canale di comunicazione fra le fork (unnamed pipe)
    int pipefd[2];

    if(pipe(pipefd) == -1){
        perror("Pipe fallita");
        exit(EXIT_FAILURE);
    }

    //Creazione delle fork
    pid_t pid_incrocio = fork(); //Fork dell'incrocio

    if (pid_incrocio == 0){//processo figlio
        incrocio(Dati_condivisi, pipefd);
        exit(0);
    }
    else if (pid_incrocio < 0){ //fork fallita
        perror("Fork incrocio fallita");
        exit(EXIT_FAILURE);
    }

    pid_t pid_garage = fork(); //Fork del garage
    
    if (pid_garage == 0){//processo figlio
        garage(Dati_condivisi, pipefd);
        exit(0);

    }
    else if(pid_garage < 0){ //fork fallita
        perror("Fork garage fallita");
        exit(EXIT_FAILURE);
    }

    //Attesa morte processi figli
    waitpid(pid_incrocio, NULL, 0);
    waitpid(pid_garage, NULL, 0);

    //Pulizia memoria condivisa e semafori

    for(int i=0; i<NUM_SEMAFORI; i++){
        if(distruzione_semaforo(&Dati_condivisi->semafori_incrocio[i]) != 0){
            perror("Errore nella distruzione del semaforo");
            exit(EXIT_FAILURE);
        }
    }

    if(distruzione_semaforo(&Dati_condivisi->ack_auto) != 0){
        perror("Errore nella distruzione del semaforo di ack");
        exit(EXIT_FAILURE);
    }

    if(clear_memory() != 0){
        perror("Errore nella pulizia della memoria condivisa e dei semafori");
        exit(EXIT_FAILURE);
    }else{
        printf("[MAIN] Tutti i processi terminati. Pulizia e chiusura effettuate.\n");
    }

    return 0;

}

void incrocio(struct Dati_condivisi* Dati_condivisi, int pipefd[]){ //Funzione dell'incrocio
    close(pipefd[1]); //Chiudo la scrittura della pipe
}

void garage(struct Dati_condivisi* Dati_condivisi, int pipefd[]){ //Funzione del garage
    close(pipefd[0]); //Chiudo la lettura della pipe
}

int distruzione_semaforo(sem_t* semaforo){

    sem_destroy(semaforo);
    return 0; //Successo
}

int clear_memory(){

    // Pulizia finale
    shm_unlink("/shm_incrocio"); // Rimuove il nome dal sistema
    return 0;
}