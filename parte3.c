/* File:
 * Compile:  mpicc -g -Wall -o parte3 parte3.c -lpthread -lrt
 * Usage:    mpiexec -n 3 ./parte3
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <mpi.h>

#define BUFFER_SIZE 6 // Númermo máximo de tarefas enfileiradas

typedef struct Relogio
{
    int p[3];
} Relogio;

typedef struct msg
{
    int origem;
    int destino;
    Relogio relogio;
} Msg;

Relogio qEntrada[BUFFER_SIZE];
Msg qSaida[BUFFER_SIZE];

Relogio relogioGlobal = {{0, 0, 0}};

int qEntradaCount = 0;
int qSaidaCount = 0;

pthread_mutex_t mutexEntrada;
pthread_mutex_t mutexSaida;

pthread_cond_t condCheioEntrada;
pthread_cond_t condVazioEntrada;

pthread_cond_t condCheioSaida;
pthread_cond_t condVazioSaida;

void printarRelogio(Relogio *relogio, int pid, int action)
{
    switch (action)
    {
    case 1:
        printf("Processo: %d | Relogio: (%d, %d, %d)\n", pid, relogio->p[0], relogio->p[1], relogio->p[2]);
        break;
    case 2:
        printf("Processo: %d | Relogio: (%d, %d, %d)\n", pid, relogio->p[0], relogio->p[1], relogio->p[2]);
        break;
    case 3:
        printf("Processo: %d | Relogio: (%d, %d, %d)\n", pid, relogio->p[0], relogio->p[1], relogio->p[2]);
        break;
    default:
        break;
    }
}

void Event(int pid, Relogio *relogio)
{
    relogio->p[pid]++;
    printarRelogio(&relogioGlobal, pid, 1);
}

void Send(int origem, int destino)
{
    pthread_mutex_lock(&mutexSaida);
    relogioGlobal.p[origem]++;
    printarRelogio(&relogioGlobal, origem, 2);
    while (qSaidaCount == BUFFER_SIZE)
    {
        pthread_cond_wait(&condCheioSaida, &mutexSaida);
    }

    Msg *msg = (Msg *)malloc(sizeof(Msg));
    msg->relogio = relogioGlobal;
    msg->origem = origem;
    msg->destino = destino;

    qSaida[qSaidaCount] = *msg;
    qSaidaCount++;

    pthread_mutex_unlock(&mutexSaida);
    pthread_cond_signal(&condVazioSaida);
}

void Receive(int pid)
{
    pthread_mutex_lock(&mutexEntrada);
    relogioGlobal.p[pid]++;
    while (qEntradaCount == 0)
    {
        pthread_cond_wait(&condVazioEntrada, &mutexEntrada);
    }

    Relogio relogio = qEntrada[0];

    for (int i = 0; i < qEntradaCount; i++)
    {
        qEntrada[i] = qEntrada[i + 1];
    }
    qEntradaCount--;

    for (int i = 0; i < 3; i++)
    {
        if (relogio.p[i] > relogioGlobal.p[i])
        {
            relogioGlobal.p[i] = relogio.p[i];
        }
    }

    printarRelogio(&relogioGlobal, pid, 3);
    pthread_mutex_unlock(&mutexEntrada);
    pthread_cond_signal(&condCheioEntrada);
}

void submitSaida()
{
    pthread_mutex_lock(&mutexSaida);

    while (qSaidaCount == 0)
    {
        pthread_cond_wait(&condVazioSaida, &mutexSaida);
    }

    int *resultados;
    resultados = calloc(3, sizeof(int));

    Msg resposta = qSaida[0];
    for (int i = 0; i < qSaidaCount - 1; i++)
    {
        qSaida[i] = qSaida[i + 1];
    }
    qSaidaCount--;

    for (int i = 0; i < 3; i++)
    {
        resultados[i] = resposta.relogio.p[i];
    }

    MPI_Send(resultados, 3, MPI_INT, resposta.destino, resposta.origem, MPI_COMM_WORLD);

    pthread_mutex_unlock(&mutexSaida);
    pthread_cond_signal(&condCheioSaida);
}

void getEntrada(long myPid)
{
    int *resultados;
    resultados = calloc(3, sizeof(int));
    Relogio *relogio = (Relogio *)malloc(sizeof(Relogio));
    MPI_Recv(resultados, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    for (int i = 0; i < 3; i++)
    {
        relogio->p[i] = resultados[i];
    }

    free(resultados);

    pthread_mutex_lock(&mutexEntrada);

    while (qEntradaCount == BUFFER_SIZE)
    {
        pthread_cond_wait(&condCheioEntrada, &mutexEntrada);
    }

    qEntrada[qEntradaCount] = *relogio;
    qEntradaCount++;

    pthread_mutex_unlock(&mutexEntrada);
    pthread_cond_signal(&condVazioEntrada);
}

void *startThreadEntrada(void *args)
{
    long myPid = (long)args;
    int cont = 0;
    switch (myPid)
    {
    case 0:
        cont = 2;
        break;
    case 1:
        cont = 2;
        break;
    case 2:
        cont = 1;
        break;

    default:
        break;
    }
    int i = 0;
    for (i; i < cont; i++){
        getEntrada(myPid);
    }
    return NULL;
}

void *startThreadMeio(void *args)
{
    long p = (long)args;

    if (p == 0)
    {
        Event(0, &relogioGlobal);
        Send(0, 1);
        Receive(0);
        Send(0, 2);
        Receive(0);
        Send(0, 1);
        Event(0, &relogioGlobal);
    }

    if (p == 1)
    {
        Send(1, 0);
        Receive(1);
        Receive(1);
    }

    if (p == 2)
    {
        Event(2, &relogioGlobal);
        Send(2, 0);
        Receive(2);
    }
    return NULL;
}

void *startThreadSaida(void *args)
{
    long myPid = (long)args;

    int cont = 0;

    switch (myPid)
    {
    case 0:
        cont = 3;
        break;
    case 1:
        cont = 1;
        break;
    case 2:
        cont = 1;
        break;

    default:
        break;
    }
    int i = 0;
    for (i; i < cont; i++){
        submitSaida();
    }

    return NULL;
}

void createThreads(long n)
{
    pthread_t tEntrada;
    pthread_t tMeio;
    pthread_t tSaida;

    pthread_cond_init(&condVazioEntrada, NULL);
    pthread_cond_init(&condVazioSaida, NULL);
    pthread_cond_init(&condCheioSaida, NULL);
    pthread_cond_init(&condCheioEntrada, NULL);
    pthread_mutex_init(&mutexEntrada, NULL);
    pthread_mutex_init(&mutexSaida, NULL);

    if (pthread_create(&tMeio, NULL, &startThreadMeio, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_create(&tEntrada, NULL, &startThreadEntrada, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_create(&tSaida, NULL, &startThreadSaida, (void *)n) != 0)
    {
        perror("Falha ao criar a thread");
    }

    if (pthread_join(tMeio, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    if (pthread_join(tEntrada, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    if (pthread_join(tSaida, NULL) != 0)
    {
        perror("Falha ao dar join na thread");
    }

    pthread_cond_destroy(&condVazioEntrada);
    pthread_cond_destroy(&condVazioSaida);
    pthread_cond_destroy(&condCheioSaida);
    pthread_cond_destroy(&condCheioEntrada);
    pthread_mutex_destroy(&mutexEntrada);
    pthread_mutex_destroy(&mutexSaida);
}

int main(int argc, char *argv[])
{
    int my_rank;

    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if (my_rank == 0)
        createThreads(0);

    else if (my_rank == 1)
        createThreads(1);

    else if (my_rank == 2)
        createThreads(2);

    MPI_Finalize();
    return 0;
}
