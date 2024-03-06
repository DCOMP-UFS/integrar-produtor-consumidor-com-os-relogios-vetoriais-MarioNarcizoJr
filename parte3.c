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

typedef struct Clock { 
   int p[3];
} Clock;

typedef struct mensagem { 
    Clock clock;
    int destino;
    int origem;
} Mensagem;

//----------------------Variáveis Globais---------------------------
Clock clockGlobal = {{0,0,0}};

int filaEntradaCont = 0;
pthread_cond_t condFullEntrada;
pthread_cond_t condEmptyEntrada;
pthread_mutex_t mutexEntrada;
Clock filaEntrada[SIZE];

int filaSaidaCont = 0;
pthread_cond_t condFullSaida;
pthread_cond_t condEmptySaida;
pthread_mutex_t mutexSaida;
Mensagem filaSaida[SIZE];


void printClock(Clock *clock, int processo) {
   printf("Process: %d, Clock: (%d, %d, %d)\n", processo, clock->p[0], clock->p[1], clock->p[2]);
}

void Event(int pid, Clock *clock){
   clock->p[pid]++;   
}


//insere mensagem com o clock, destino e origem na fila de saida
void Send(int origem, int destino) {
        pthread_mutex_lock(&mutexSaida);//faz o lock da fila de saída
        clockGlobal.p[origem]++;
        printClock(&clockGlobal, origem);

        while(filaSaidaCont == SIZE) { //enquanto estiver cheia espere
            pthread_cond_wait(&condFullSaida, &mutexSaida);
        }
        
        //cria a mensagem
        Mensagem *mensagem = (Mensagem*)malloc(sizeof(Mensagem));
        mensagem->clock = clockGlobal;
        mensagem->origem = origem;
        mensagem->destino = destino;
        
        //insere na fila
        filaSaida[filaSaidaCont] = *mensagem;
        filaSaidaCont++;
        
        pthread_mutex_unlock(&mutexSaida); //faz o unlock da fila de saída
        pthread_cond_signal(&condEmptySaida); //fila não está mais vazia
}

void saidaSend() {
    pthread_mutex_lock(&mutexSaida); //faz o lock na fila de saida
    
    while(filaSaidaCont == 0) { //enquanto estiver vazia espere
        pthread_cond_wait(&condEmptySaida, &mutexSaida);
    }
    
    //tira do começo da fila
    Mensagem mensagem = filaSaida[0];
    for (int i = 0; i < filaSaidaCont - 1; i++) {
        filaSaida[i] = filaSaida[i+1];
    }
    filaSaidaCont--;
    
    int *valoresClock; //valores para enviar no MPI_Send
    valoresClock = calloc(3, sizeof(int));
   
    for (int i = 0; i < 3; i++) { //coloca o clock atual nos valores a enviar
        valoresClock[i] = mensagem.clock.p[i];
    }   
    //printf("Enviando o clock {%d, %d, %d} do processo %d para o processo %d\n", clock->p[0], clock->p[1], clock->p[2], origem, destino);

    MPI_Send(valoresClock, 3, MPI_INT, mensagem.destino, mensagem.origem, MPI_COMM_WORLD);
   
    free(valoresClock);
    
    pthread_mutex_unlock(&mutexSaida); //faz o unlock na fila de entrada
    pthread_cond_signal(&condFullSaida); //fila não está mais cheia
}

void entradaReceive() {
        int *valoresClock; //valores pra receber o clock
        valoresClock = calloc (3, sizeof(int));
        Clock *clock = (Clock*)malloc(sizeof(Clock));
        MPI_Recv(valoresClock, 3,  MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        for (int i = 0; i < 3; i++) {//coloca os valores recebidos em um clock
                clock->p[i] = valoresClock[i];
        }
        free(valoresClock);
        
        pthread_mutex_lock(&mutexEntrada); //faz o lock da fila de entrada
        
        while(filaEntradaCont == SIZE) { //enquanto estiver cheia espere
            pthread_cond_wait(&condFullEntrada, &mutexEntrada);
        }
        
        //insere clock no começo da fila
        filaEntrada[filaEntradaCont] = *clock;
        filaEntradaCont++;
        
        pthread_mutex_unlock(&mutexEntrada); //faz o unlock da fila de entrada
        pthread_cond_signal(&condEmptyEntrada); //fila não está mais vazia
}

//retira clock da fila de entrada
void Receive(int processo) {
    pthread_mutex_lock(&mutexEntrada); //faz o lock na fila de entrada
    clockGlobal.p[processo]++;
    
    while(filaEntradaCont == 0) { //enquanto estiver vazia espere
        pthread_cond_wait(&condEmptyEntrada, &mutexEntrada);
    }
    
    //tira do começo da fila
    Clock clock = filaEntrada[0];
    for (int i = 0; i < filaEntradaCont -1; i++) {
        filaEntrada[i] = filaEntrada[i+1];
    }
    filaEntradaCont--;
    
    for (int i = 0; i < 3; i++) { //atualiza o clock da thread relogio
        if(clock.p[i] > clockGlobal.p[i]) {
            clockGlobal.p[i] = clock.p[i];
        }
    }

    printClock(&clockGlobal, processo); //printa o clock atualizado
    
    pthread_mutex_unlock(&mutexEntrada); //faz o unlock na fila de entrada
    pthread_cond_signal(&condFullEntrada); //fila não está mais cheia
}

void* threadRelogio(void* arg) {
    long p = (long) arg;
    if (p == 0) {
        Event(0, &clockGlobal);
        printClock(&clockGlobal, 0);
        
        Send(0, 1); //envia do processo 0 ao processo 1
        
        Receive(0); //recebe
        
        Send(0, 2); //envia do processo 0 ao processo 2
        
        Receive(0); //recebe
        
        Send(0, 1); //envia do processo 0 ao processo 1

        Event(0, &clockGlobal);
        printClock(&clockGlobal, 0);
    }
    
    if (p == 1) {
        Send(1, 0); //envia do processo 1 ao processo 0

        Receive(1); //recebe

        Receive(1); //recebe
    }

    if (p == 2) {
        Event(2, &clockGlobal);
        printClock(&clockGlobal, 2);

        Send(2, 0); //envia do processo 2 ao processo 0

        Receive(2); //recebe
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
    
    //inicializações
    pthread_cond_init(&condFullEntrada, NULL);
    pthread_cond_init(&condEmptyEntrada, NULL);
    pthread_cond_init(&condFullSaida, NULL);
    pthread_cond_init(&condEmptySaida, NULL);
    pthread_mutex_init(&mutexEntrada, NULL);
    pthread_mutex_init(&mutexSaida, NULL);
    

    //cria threads
    if (pthread_create(&tRelogio, NULL, &threadRelogio, (void*) p) != 0) { //cria thread Relogio
        perror("Failed to create the thread");
    }     
    if (pthread_create(&tEntrada, NULL, &threadEntrada, (void*) p) != 0) { //cria thread de entrada
        perror("Failed to create the thread");
    }  
    if (pthread_create(&tSaida, NULL, &threadSaida, (void*) p) != 0) { //cria thread de saida
        perror("Failed to create the thread");
    }  
    
    //join das threads 
    if (pthread_join(tRelogio, NULL) != 0) { //join thread Relogio
        perror("Failed to join the thread");
    }  
    if (pthread_join(tEntrada, NULL) != 0) { //join threads entrada
        perror("Failed to join the thread");
    }  
    if (pthread_join(tSaida, NULL) != 0) { //join threads saida
        perror("Failed to join the thread");
    } 
    
    //destroi as condições e mutex

    pthread_cond_destroy(&condFullEntrada);
    pthread_cond_destroy(&condEmptyEntrada);
    pthread_cond_destroy(&condFullSaida);
    pthread_cond_destroy(&condEmptySaida);
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
