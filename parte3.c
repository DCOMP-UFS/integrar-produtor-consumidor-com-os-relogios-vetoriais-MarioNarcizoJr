#include <stdio.h>
#include <stdlib.h>
#include <pthread.h> 
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <mpi.h> 

// * Compilação: mpicc -o parte3 parte3.c  -lpthread -lrt
// * Execução:   mpiexec -n 3 ./parte3 
// wsl: mpiexec --host $(hostnamectl hostname):3 -n 3 parte3
/*

- tirar codigos duplicados do processo
- funcções separadas de inserir e retirar da fila
*/

#define SIZE 10 //tamanho das filas

typedef struct Relogio { 
   int p[3];
} Relogio;

typedef struct msg { 
    Relogio relogio;
    int destino;
    int origem;
} Msg;

Relogio relogioGlobal = {{0,0,0}};

int filaEntradaCont = 0;
pthread_cond_t condCheioEntrada;
pthread_cond_t condVazioEntrada;
pthread_mutex_t mutexEntrada;
Relogio filaEntrada[SIZE];

int filaSaidaCont = 0;
pthread_cond_t condCheioSaida;
pthread_cond_t condVazioSaida;
pthread_mutex_t mutexSaida;
Msg filaSaida[SIZE];

void printClock(Relogio *relogio, int processo) {
   printf("Process: %d, Relogio: (%d, %d, %d)\n", processo, relogio->p[0], relogio->p[1], relogio->p[2]);
}

void Event(int pid, Relogio *relogio){
   relogio->p[pid]++;   
}

void Send(int origem, int destino) {
        pthread_mutex_lock(&mutexSaida);
        relogioGlobal.p[origem]++;
        printClock(&relogioGlobal, origem);

        while(filaSaidaCont == SIZE) { 
            pthread_cond_wait(&condCheioSaida, &mutexSaida);
        }
        
        Msg *msg = (Msg*)malloc(sizeof(Msg));
        msg->relogio = relogioGlobal;
        msg->origem = origem;
        msg->destino = destino;
        
        filaSaida[filaSaidaCont] = *msg;
        filaSaidaCont++;
        
        pthread_mutex_unlock(&mutexSaida);
        pthread_cond_signal(&condVazioSaida);
}

void saidaSend() {
    pthread_mutex_lock(&mutexSaida);
    
    while(filaSaidaCont == 0) {
        pthread_cond_wait(&condVazioSaida, &mutexSaida);
    }
    
    Msg msg = filaSaida[0];
    for (int i = 0; i < filaSaidaCont - 1; i++) {
        filaSaida[i] = filaSaida[i+1];
    }
    filaSaidaCont--;
    
    int *valoresRelogio;
    valoresRelogio = calloc(3, sizeof(int));
   
    for (int i = 0; i < 3; i++) {
        valoresRelogio[i] = msg.relogio.p[i];
    }   
    //printf("Enviando o clock {%d, %d, %d} do processo %d para o processo %d\n", clock->p[0], clock->p[1], clock->p[2], origem, destino);

    MPI_Send(valoresRelogio, 3, MPI_INT, msg.destino, msg.origem, MPI_COMM_WORLD);
   
    free(valoresRelogio);
    
    pthread_mutex_unlock(&mutexSaida); //faz o unlock na fila de entrada
    pthread_cond_signal(&condCheioSaida); //fila não está mais cheia
}

void entradaReceive() {
        int *valoresRelogio;
        valoresRelogio = calloc (3, sizeof(int));
        Relogio *relogio = (Relogio*)malloc(sizeof(Relogio));
        MPI_Recv(valoresRelogio, 3,  MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        for (int i = 0; i < 3; i++) {
                relogio->p[i] = valoresRelogio[i];
        }
        free(valoresRelogio);
        
        pthread_mutex_lock(&mutexEntrada);
        
        while(filaEntradaCont == SIZE) {
            pthread_cond_wait(&condCheioEntrada, &mutexEntrada);
        }
   
        filaEntrada[filaEntradaCont] = *relogio;
        filaEntradaCont++;
        
        pthread_mutex_unlock(&mutexEntrada);
        pthread_cond_signal(&condVazioEntrada);
}

void Receive(int processo) {
    pthread_mutex_lock(&mutexEntrada)
    relogioGlobal.p[processo]++;
    
    while(filaEntradaCont == 0) {
        pthread_cond_wait(&condVazioEntrada, &mutexEntrada);
    }
    
    Relogio relogio = filaEntrada[0];
    for (int i = 0; i < filaEntradaCont -1; i++) {
        filaEntrada[i] = filaEntrada[i+1];
    }
    filaEntradaCont--;
    
    for (int i = 0; i < 3; i++) {
        if(relogio.p[i] > relogioGlobal.p[i]) {
            relogioGlobal.p[i] = relogio.p[i];
        }
    }

    printClock(&relogioGlobal, processo);
    
    pthread_mutex_unlock(&mutexEntrada);
    pthread_cond_signal(&condCheioEntrada);
}

void* threadRelogio(void* arg) {
    long p = (long) arg;
    if (p == 0) {
        Event(0, &relogioGlobal);
        printClock(&relogioGlobal, 0);
        
        Send(0, 1);
        
        Receive(0);
        
        Send(0, 2);
        
        Receive(0);
        
        Send(0, 1);

        Event(0, &relogioGlobal);
        printClock(&relogioGlobal, 0);
    }
    
    if (p == 1) {
        Send(1, 0);

        Receive(1);

        Receive(1);
    }

    if (p == 2) {
        Event(2, &relogioGlobal);
        printClock(&relogioGlobal, 2);

        Send(2, 0);

        Receive(2);
    }
    return NULL;
}

void* threadSaida(void* arg) {
    long p = (long) arg;
    while(1) {
        saidaSend();
    }
    return NULL;
}

void* threadEntrada(void* arg) {
    long p = (long) arg;
    while(1) {
        entradaReceive();
    }
    return NULL;
}

void processo(long p) {
    pthread_t tSaida; 
    pthread_t tEntrada;
    pthread_t tRelogio;
    
    pthread_cond_init(&condCheioEntrada, NULL);
    pthread_cond_init(&condVazioEntrada, NULL);
    pthread_cond_init(&condCheioSaida, NULL);
    pthread_cond_init(&condVazioSaida, NULL);
    pthread_mutex_init(&mutexEntrada, NULL);
    pthread_mutex_init(&mutexSaida, NULL);
    

    if (pthread_create(&tRelogio, NULL, &threadRelogio, (void*) p) != 0) {
        perror("Failed to create the thread");
    }     
    if (pthread_create(&tEntrada, NULL, &threadEntrada, (void*) p) != 0) { 
    }  
    if (pthread_create(&tSaida, NULL, &threadSaida, (void*) p) != 0) {
        perror("Failed to create the thread");
    }  
    
    if (pthread_join(tRelogio, NULL) != 0) { 
        perror("Failed to join the thread");
    }  
    if (pthread_join(tEntrada, NULL) != 0) {
        perror("Failed to join the thread");
    }  
    if (pthread_join(tSaida, NULL) != 0) {
        perror("Failed to join the thread");
    } 

    pthread_cond_destroy(&condCheioEntrada);
    pthread_cond_destroy(&condVazioEntrada);
    pthread_cond_destroy(&condCheioSaida);
    pthread_cond_destroy(&condVazioSaida);
    pthread_mutex_destroy(&mutexEntrada);
    pthread_mutex_destroy(&mutexSaida);
}



int main(void) {
   int my_rank;               

   MPI_Init(NULL, NULL); 
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); 

   if (my_rank == 0) { 
      processo(0);
   } else if (my_rank == 1) {  
      processo(1);
   } else if (my_rank == 2) {  
      processo(2);
   }

   /* Finaliza MPI */
   MPI_Finalize(); 

   return 0;
}  /* main */
