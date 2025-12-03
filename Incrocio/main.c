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
#define NUM_MACCHINE 4
#define NUM_DIREZIONI 4
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

    if(clear_memory() != 0){
        perror("Errore nella pulizia della memoria condivisa e dei semafori");
        exit(EXIT_FAILURE);
    }else{
        printf("[MAIN] Tutti i processi terminati. Pulizia e chiusura effettuate.\n");
    }

    return 0;

}

void automobile(struct Dati_condivisi *Dati_condivisi, int id_macchina, int direzione){ //Funzione dell'automobile

    printf("[AUTO %d] In arrivo all'incrocio, direzione: %d.\n", id_macchina, direzione);

    //Step 1: Attendere il semaforo verde
    sem_wait(&Dati_condivisi->semafori_incrocio[id_macchina]);
    printf("[AUTO %d] Passando l'incrocio.\n", id_macchina);

    //Step 2: Loggare l'evento su file di testo
    // Apro il file in modalità scrittura, append e creo se non esiste
    int fd = open(FILE_AUTO, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1)
    {
        perror("Errore apertura file auto");
        exit(EXIT_FAILURE);
    }
    // Scrivo sul file di testo
    char sBuffer[100]; // 1. Creo lo spazio
    int lunghezza = sprintf(sBuffer, "L'auto %d proviene dalla direzione %d\n", id_macchina, direzione); // 2. Compilo lo spazio
    if ((write(fd, sBuffer, lunghezza)) != lunghezza)
    {
        perror("Errore scrittura su file auto");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd); // Chiudo il file

    //Step 3: Segnalare l'avvenuto passaggio
    sem_post(&Dati_condivisi->ack_auto); //Invio l'ack
    printf("[AUTO %d] Passaggio completato, ack inviato.\n", id_macchina);

}

void garage(struct Dati_condivisi* Dati_condivisi, int pipefd[]){ //Funzione del garage
    close(pipefd[0]); //Chiudo la lettura della pipe

    while(1){
        pid_t auto_fork[NUM_MACCHINE];
        int direzioni[NUM_DIREZIONI];

        printf("\n[GARAGE] --- Generazione nuove auto ---\n");

        //Estraggo le direzioni per le future automobili
        for(int i=0; i<NUM_DIREZIONI; i++){
            direzioni[i] = EstraiDirezione(i);
        }

        // Invio le direzioni alle automobili tramite la pipe NELLA pipe
        if (write(pipefd[1], direzioni, sizeof(direzioni)) == -1) {
            perror("Errore nella write del Garage"); // Gestione errore standard
            exit(EXIT_FAILURE);
        }

        //Creazione delle fork per le automobili
        for(int i=0; i<NUM_MACCHINE; i++){
            pid_t pid_auto = fork(); //Fork dell'automobile
            auto_fork[i] = pid_auto;
            if (pid_auto == 0){//processo figlio
                close(pipefd[1]); //Chiudo la scrittura della pipe
                automobile(Dati_condivisi, i , direzioni[i]);
                exit(0);
            }
            else if(pid_auto > 0){//fork riuscita e il padre tiene traccia delle fork create
                auto_fork[i] = pid_auto;
            }else{
                perror("Fork automobile fallita");
                exit(EXIT_FAILURE);
            }
        }

        // Attendo la terminazione di tutte le automobili create
        for (int i = 0; i < NUM_MACCHINE; i++)
        {
            waitpid(auto_fork[i], NULL, 0);
        }

        // Attesa passiva prima del prossimo ciclo (come da specifiche)
        sleep(1);
    }
}

void incrocio(struct Dati_condivisi *Dati_condivisi, int pipefd[]) // Funzione dell'incrocio
{
    close(pipefd[1]); // Chiudo la scrittura della pipe

    int direzioni[NUM_DIREZIONI];

    while (1)
    {

        if (read(pipefd[0], direzioni, sizeof(direzioni)) > 0) // Leggo le direzioni dalla pipe
        {
            printf("\n[INCROCIO] --- Nuove auto in arrivo all'incrocio ---\n");

            for (int k = 0; k < NUM_MACCHINE; k++)
            {
                //Step 1: Determinare quale auto può passare
                int id_macchina = GetNextCar(direzioni); //Restituisce l'indice della prossima auto che può passare
                printf("[INCROCIO] L'auto proveniente dalla strada %d puo' passare.\n", id_macchina);

                //Step 2: Loggare l'evento su file di testo
                // Apro il file in modalità scrittura, append e creo se non esiste
                int fd = open(FILE_INCROCIO, O_WRONLY | O_APPEND | O_CREAT, 0666);
                if (fd == -1)
                {
                    perror("Errore apertura file incrocio");
                    exit(EXIT_FAILURE);
                }

                // Scrivo sul file di testo
                char sBuffer[100]; // 1. Creo lo spazio
                int lunghezza = sprintf(sBuffer, "L'auto %d e' passata\n", id_macchina); // 2. Compilo lo spazio
                if ((write(fd, sBuffer, lunghezza)) != lunghezza)
                {
                    perror("Errore scrittura su file incrocio");
                    close(fd);
                    exit(EXIT_FAILURE);
                }
                close(fd); // Chiudo il file

                //Step 3: Aggiornare lo stato delle macchine
                sem_post(&Dati_condivisi->semafori_incrocio[id_macchina]); //L'auto puo' passare (verde)

                //Step 4: Attendere l'ack dall'auto
                sem_wait(&Dati_condivisi->ack_auto); //Attendo l'ack dall'auto
                printf("[INCROCIO] Ack ricevuto dall'auto %d.\n", id_macchina);

                //Step 5: Aggiornare le direzioni
                direzioni[id_macchina] = -1; //L'auto e' passata, quindi aggiorno la direzione
                }
            }
        }
    }

int clear_memory()
{

    // Pulizia finale

    sem_destroy(&Dati_condivisi->ack_auto);
    for(int i=0; i<NUM_SEMAFORI; i++) {
        sem_destroy(&Dati_condivisi->semafori_incrocio[i]);
    }

    shm_unlink("/shm_incrocio"); // Rimuove il nome dal sistema
    return 0;
}

//Aggiungere terminazione sigterm
//Script bash per compilare e avviare il programma