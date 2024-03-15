/*
 * Compile:  mpicc -g -Wall -o parte3 parte3.c -lpthread -lrt
 * Usage:    mpiexec -n 3 ./parte3
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h> 
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <mpi.h>     

#define NTHREADS 3
#define FILA_SIZE 10

typedef struct {
    int p[3];
    int idProcess;
} Relogio;

pthread_mutex_t saidaMUTEX;
pthread_cond_t saidaVAZIA;
pthread_cond_t saidaCHEIO;

int saidacontRelogio = 0;
Relogio saidaClockCont[FILA_SIZE];

pthread_mutex_t entradaMUTEX;
pthread_cond_t entradaVAZIA;
pthread_cond_t entradaCHEIO;
int entradacontRelogio = 0;
Relogio entradafilaRelogio[FILA_SIZE];

void Event(int pid, Relogio *relogio) {
    relogio->p[pid]++;
    printf("Processo: %d, Relógio: (%d, %d, %d)\n", pid, relogio->p[0], relogio->p[1], relogio->p[2]);
}

Relogio GetClock(pthread_mutex_t *mutex, pthread_cond_t *condVAZIO, pthread_cond_t *condCHEIO, int *contRelogio, Relogio *filaRelogio) {
    Relogio relogio;
    pthread_mutex_lock(mutex);
    
    while (*contRelogio == 0) {
        pthread_cond_wait(condVAZIO, mutex);
    }

    relogio = filaRelogio[0];

    for (int i = 0; i < *contRelogio - 1; i++) {
        filaRelogio[i] = filaRelogio[i + 1];
    }

    (*contRelogio)--;
    
    pthread_mutex_unlock(mutex);

    pthread_cond_signal(condCHEIO);
    
    return relogio;
}

void PutClock(pthread_mutex_t *mutex, pthread_cond_t *condVAZIO, pthread_cond_t *condCHEIO, int *contRelogio, Relogio relogio, Relogio *filaRelogio) {
    pthread_mutex_lock(mutex);

    while (*contRelogio == FILA_SIZE) {
        pthread_cond_wait(condCHEIO, mutex);
    }
    
    Relogio temp = relogio;

    filaRelogio[*contRelogio] = temp;
    (*contRelogio)++;
    

    pthread_mutex_unlock(mutex);
    pthread_cond_signal(condVAZIO);
}

void SendControl(int id, Relogio *relogio) {
    Event(id, relogio);
    PutClock(&saidaMUTEX, &saidaVAZIA, &saidaCHEIO, &saidacontRelogio, *relogio, saidaClockCont);
}

Relogio* ReceiveControl(int id, Relogio *relogio) {
    Relogio* temp = relogio;
    Relogio relogio2 = GetClock(&entradaMUTEX, &entradaVAZIA, &entradaCHEIO, &entradacontRelogio, entradafilaRelogio);
    for (int i = 0; i < 3; i++) {
        if (temp->p[i] < relogio2.p[i]) {
            temp->p[i] = relogio2.p[i];
        }
    }
    temp->p[id]++;
    printf("Processo: %d, Relógio: (%d, %d, %d)\n", id, relogio->p[0], relogio->p[1], relogio->p[2]);
    return temp;
}

void Send(int pid, Relogio *relogio){
    int mensagem[3];
    mensagem[0] = relogio->p[0];
    mensagem[1] = relogio->p[1];
    mensagem[2] = relogio->p[2];
    //MPI SEND
    MPI_Send(&mensagem, 3, MPI_INT, relogio->idProcess, 0, MPI_COMM_WORLD);
}

void Receive(int pid, Relogio *relogio){
    int mensagem[3];
    //MPI RECV
    MPI_Recv(&mensagem, 3, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    relogio->p[0] = mensagem[0];
    relogio->p[1] = mensagem[1];
    relogio->p[2] = mensagem[2];
}

void *MainThread(void *args) {
    long id = (long) args;
    int pid = (int) id;
    Relogio* relogio = malloc(sizeof(Relogio));
    
    // Inicializando os campos da estrutura Relogio
    relogio->p[0] = 0;
    relogio->p[1] = 0;
    relogio->p[2] = 0;
    relogio->idProcess = 0;
        
    if (pid == 0) {
        
        Event(pid, relogio);
        
      
        relogio->idProcess = 1;
        SendControl(pid, relogio);
       
        relogio = ReceiveControl(pid, relogio);
        
      
        relogio->idProcess = 2;
        SendControl(pid, relogio);
       
        relogio = ReceiveControl(pid, relogio);
        
        
        relogio->idProcess = 1;
        SendControl(pid, relogio);
    
       
        Event(pid, relogio);
    } else if (pid == 1) {
        
        relogio->idProcess = 0;
        SendControl(pid, relogio);
        
     
        relogio = ReceiveControl(pid, relogio);
        

        relogio = ReceiveControl(pid, relogio);
    } else if (pid == 2) {
    
        Event(pid, relogio);
        
    
        relogio->idProcess = 0;
        SendControl(pid, relogio);
     
        relogio = ReceiveControl(pid, relogio);
    }

    return NULL;
}

void *SendThread(void *args) {
    long pid = (long) args;
    Relogio relogio;
    
    while(1){
      relogio = GetClock(&saidaMUTEX, &saidaVAZIA, &saidaCHEIO, &saidacontRelogio, saidaClockCont);
      Send(pid, &relogio);
    }

    return NULL;
}

void *ReceiveThread(void *args) {
    long pid = (long) args;
    Relogio relogio;

    while(1){
      Receive(pid, &relogio);
      PutClock(&entradaMUTEX, &entradaVAZIA, &entradaCHEIO, &entradacontRelogio, relogio, entradafilaRelogio);
    }
 
    return NULL;
}

// Representa o processo de rank 0
void process0(){
   pthread_t thread[NTHREADS];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 0);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 0);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 0);

   for (int i = 0; i < NTHREADS; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

// Representa o processo de rank 1
void process1(){
   pthread_t thread[NTHREADS];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 1);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 1);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 1);
   
   for (int i = 0; i < NTHREADS; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

// Representa o processo de rank 2
void process2(){
   pthread_t thread[NTHREADS];
   pthread_create(&thread[0], NULL, &MainThread, (void*) 2);
   pthread_create(&thread[1], NULL, &SendThread, (void*) 2);
   pthread_create(&thread[2], NULL, &ReceiveThread, (void*) 2);
   
   for (int i = 0; i < NTHREADS; i++){  
      if (pthread_join(thread[i], NULL) != 0) {
         perror("Falha ao juntar a thread");
      }
   }
}

int main(int argc, char* argv[]) {
   int my_rank;
   
   pthread_mutex_init(&entradaMUTEX, NULL);
   pthread_mutex_init(&saidaMUTEX, NULL);
   pthread_cond_init(&entradaVAZIA, NULL);
   pthread_cond_init(&saidaVAZIA, NULL);
   pthread_cond_init(&entradaCHEIO, NULL);
   pthread_cond_init(&saidaCHEIO, NULL);
  
   
   MPI_Init(NULL, NULL); 
   MPI_Comm_rank(MPI_COMM_WORLD, &my_rank); 

   if (my_rank == 0) { 
      process0();
   } else if (my_rank == 1) {  
      process1();
   } else if (my_rank == 2) {  
      process2();
   }

   /* Finaliza MPI */
   
   
   pthread_mutex_destroy(&entradaMUTEX);
   pthread_mutex_destroy(&saidaMUTEX);
   pthread_cond_destroy(&entradaVAZIA);
   pthread_cond_destroy(&saidaVAZIA);
   pthread_cond_destroy(&entradaCHEIO);
   pthread_cond_destroy(&saidaCHEIO);
   
   MPI_Finalize();

   return 0;
} 
