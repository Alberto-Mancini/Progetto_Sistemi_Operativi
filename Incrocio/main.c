#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include "incrocio.h"

#define NUM_SEMAFORI 4
#define NUM_MACCHINE 4
#define NUM_DIREZIONI 4
#define FILE_INCROCIO "incrocio.txt"
#define FILE_AUTO "auto.txt"

struct Dati_condivisi{
    sem_t semafori_incrocio[4];
    sem_t ack_auto;
    pid_t pid_garage;
    pid_t pid_incrocio;
};

struct Dati_condivisi* pDati;

// --- FUNZIONI --- //
void incrocio(int pipefd[]);
void garage(int pipefd[]);
void automobile(int id_macchina, int direzione);
int clear_memory();

// --- GESTORI SEGNALI --- //
void gestore_garage(int sig) {
    if (sig == SIGTERM) {
        exit(0); 
    }
}

void gestore_incrocio(int sig) {
    if (sig == SIGTERM) {
        printf("\n[INCROCIO] Ricevuto SIGTERM. Avvio procedura di terminazione...\n");
        
        // 1. Informa il garage che deve terminare 
        kill(pDati->pid_garage, SIGTERM);
        
        // 2. Attende la terminazione del garage
        waitpid(pDati->pid_garage, NULL, 0);
        
        printf("[INCROCIO] Garage terminato. Termino anche io.\n");
        // 3. Termina se stesso
        exit(0);
    }
}

int main(){
    
    shm_unlink("/shm_incrocio"); //Pulizia vecchie zone di memoria

    int area_shm = shm_open("/shm_incrocio", O_CREAT | O_RDWR, 0666); //Apriamo una zona di memoria condivisa
    if (area_shm == -1) { perror("Errore shm_open"); exit(EXIT_FAILURE); }

    if (ftruncate(area_shm, sizeof(struct Dati_condivisi)) == -1) { 
        perror("Errore ftruncate"); exit(EXIT_FAILURE); 
    }

    pDati = mmap(NULL, sizeof(struct Dati_condivisi), PROT_READ | PROT_WRITE, MAP_SHARED, area_shm, 0); //Mappiamo la memoria condivisa
    if (pDati == MAP_FAILED) { perror("Errore mmap"); exit(EXIT_FAILURE); }
    close(area_shm);

    // Inizializzazione semafori
    for(int i=0; i<NUM_SEMAFORI; i++){
        sem_init(&pDati->semafori_incrocio[i],1,0); 
    }
    sem_init(&pDati->ack_auto,1,0); 

    // Reset file di log all'avvio
    remove(FILE_INCROCIO);
    remove(FILE_AUTO);

    int pipefd[2];
    if(pipe(pipefd) == -1){ perror("Pipe fallita"); exit(EXIT_FAILURE); }

    // --- CREAZIONE PROCESSI ---
    pid_t pid_incrocio = fork(); 
    if (pid_incrocio == 0){
        // Codice figlio INCROCIO
        incrocio(pipefd);
        exit(0);
    }
    else if (pid_incrocio < 0){ perror("Fork incrocio fallita"); exit(EXIT_FAILURE); }

    pid_t pid_garage = fork(); 
    if (pid_garage == 0){
        // Codice figlio GARAGE
        garage(pipefd);
        exit(0);
    }
    else if(pid_garage < 0){ perror("Fork garage fallita"); exit(EXIT_FAILURE); }

    // Salviamo i PID nella memoria condivisa appena li conosco
    pDati->pid_incrocio = pid_incrocio;
    pDati->pid_garage = pid_garage;

    printf("[MAIN] Processi avviati. PID Incrocio: %d, PID Garage: %d\n", pid_incrocio, pid_garage);
    printf("[MAIN] Per terminare: kill -SIGTERM %d\n", pid_incrocio);

    waitpid(pid_garage, NULL, 0);
    waitpid(pid_incrocio, NULL, 0);

    // Pulizia finale
    if(clear_memory() != 0){
        perror("Errore pulizia");
        exit(EXIT_FAILURE);
    } else {
        printf("[MAIN] Pulizia completata. Bye.\n");
    }

    return 0;
}

void automobile(int id_macchina, int direzione){ 
    // Step 1: Attendere semaforo verde
    sem_wait(&pDati->semafori_incrocio[id_macchina]);
    
    // Step 2: Loggare su file
    int fd = open(FILE_AUTO, O_WRONLY | O_APPEND | O_CREAT, 0666);
    if (fd == -1) { perror("Errore file auto"); exit(EXIT_FAILURE); }
    
    char sBuffer[100];
    // Scriviamo la strada di provenienza (che corrisponde a id_macchina)
    int lunghezza = sprintf(sBuffer, "Strada provenienza: %d\n", id_macchina); 
    write(fd, sBuffer, lunghezza);
    close(fd); 

    // Step 3: Ack
    sem_post(&pDati->ack_auto); 
}

void garage(int pipefd[]){ 
    close(pipefd[0]); 
    signal(SIGTERM, gestore_garage); 

    while(1){
        pid_t auto_fork[NUM_MACCHINE];
        int direzioni[NUM_DIREZIONI];

        for(int i=0; i<NUM_DIREZIONI; i++) direzioni[i] = EstraiDirezione(i);

        // Scrittura pipe
        if (write(pipefd[1], direzioni, sizeof(direzioni)) == -1) {
            if (errno == EINTR) continue; // Se interrotto da segnale, riprova o esci al check loop
            perror("Errore write garage"); exit(EXIT_FAILURE);
        }

        for(int i=0; i<NUM_MACCHINE; i++){
            pid_t pid_auto = fork(); 
            if (pid_auto == 0){
                close(pipefd[1]); 
                automobile(i , direzioni[i]);
                exit(0);
            }
            else if(pid_auto > 0){
                auto_fork[i] = pid_auto;
            } else {
                perror("Fork auto fallita"); exit(EXIT_FAILURE);
            }
        }

        // Attende terminazione automobili
        for (int i = 0; i < NUM_MACCHINE; i++) {
            waitpid(auto_fork[i], NULL, 0);
        }

        sleep(1); 
    }
}

void incrocio(int pipefd[]) 
{
    close(pipefd[1]); 
    signal(SIGTERM, gestore_incrocio);

    int direzioni[NUM_DIREZIONI];

    while (1)
    {
        ssize_t n = read(pipefd[0], direzioni, sizeof(direzioni));
        
        if (n > 0) 
        {
            for (int k = 0; k < NUM_MACCHINE; k++)
            {
                int id_macchina = GetNextCar(direzioni); 
                
                int fd = open(FILE_INCROCIO, O_WRONLY | O_APPEND | O_CREAT, 0666);
                if (fd == -1) { perror("Errore file incrocio"); exit(EXIT_FAILURE); }

                char sBuffer[100];
                int lunghezza = sprintf(sBuffer, "Strada provenienza: %d\n", id_macchina); 
                write(fd, sBuffer, lunghezza);
                close(fd); 

                printf("[INCROCIO] Passa auto %d (va in %d)\n", id_macchina, direzioni[id_macchina]);
                
                sem_post(&pDati->semafori_incrocio[id_macchina]); 
                sem_wait(&pDati->ack_auto); 
                
                direzioni[id_macchina] = -1; 
            }
        }
    }
}

int clear_memory()
{
    sem_destroy(&pDati->ack_auto);
    for(int i=0; i<NUM_SEMAFORI; i++) {
        sem_destroy(&pDati->semafori_incrocio[i]);
    }
    shm_unlink("/shm_incrocio"); 
    return 0;
}