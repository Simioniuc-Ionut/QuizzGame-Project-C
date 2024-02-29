#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <pthread.h>

#include <sys/select.h>
//./QuizzClient 127.0.0.1 2000
/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;


typedef struct {
    //Intrebarea curenta
     char question[200];
     char option[10][200];
     char id_question[20];
     char corect[10];
}Questions;

typedef struct {
  int scor;
  char username[100];
  int nr_inregistrare;

  long secunde;
  long milisecunde;
  long microsecunde;

  int activ;
}Client;

typedef struct{
  int questions;
  int threads;
}Count;

int answerDone=0;

ssize_t primesteDate(int socket, Client *clients, int nrClients) {
    size_t lungimeClient = sizeof(Client);
    for (int i = 0; i < nrClients; i++) {
        char *ptr = (char *)&clients[i];
        size_t primit = 0;

        while (primit < lungimeClient) {
            ssize_t rezultat = read(socket, ptr + primit, lungimeClient - primit);
            if (rezultat <= 0) {
                // Eroare sau conexiune închisă
                printf("Eroare in read in functia de primesteDate\n");
                return rezultat;
            }
            primit += rezultat;
        }
    }

    return nrClients * lungimeClient;
}

void *Cronometru(void* durata){
  int timpDeRaspuns = *((int*) durata);

  while(timpDeRaspuns>0 && answerDone !=1){
     //printf("\033[%d;0H", linieCronometru); // Muta cursorul la linia cronometrului
     printf("\rTimp ramas: %d secunde ", timpDeRaspuns); // Rescrie cronometrul
     fflush(stdout);
        sleep(1);
        timpDeRaspuns--;
  }
  printf("\r");
    pthread_exit(NULL);
}

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  Questions intrebari={0};
  Client client={0};//initializam toate campurile cu 0
  Client *clients=NULL;
  Count nr={0};
  int nrquestions={0};
  int nrthreads_actualizat={0};
  int timpCronometru={0},linieCronometru={0};

  char varianta_raspuns[4]={0};
  char Wait_Start[]={"Se asteapta alti jucatori ..."};
  //char username[]={"Domnul G."};
  char username[20]={0};
  //parte de timeout
  struct timeval TimpDeRaspuns={0};
  
   pthread_t threadCronometru;
  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr(argv[1]);
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
    {
      perror ("[client]Eroare la connect().\n");
      return errno;
    }
/* citirea raspunsului dat de server 
     (apel blocant pina cand serverul raspunde) */

    printf("%s\n",Wait_Start);


     if (read (sd, &nr,sizeof(Count)) < 0)
      {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
      }
      nrquestions=nr.questions;

  if(nrquestions == -1){
    printf("Quizz ul deja a inceput , incercati mai tarziu! \n");
  }
  else
  { 

      printf("nrquestions %d nrthreads %d\n",nrquestions,nr.threads);

    /*citesc timpul de raspuns primit de la server*/
      if (read (sd, &TimpDeRaspuns,sizeof(struct timeval)) < 0)
      {
        perror ("[client]Eroare la read() de la server.\n");
        return errno;
      }
   

     printf("START!! \n");

    int contor=0;
   
    printf("Introduceti username: \n");
    fflush(stdout);
    read(0,username,sizeof(username));
     strcpy(client.username,username);

      if (write (sd,&client,sizeof(Client)) <= 0)
             {
               perror ("[client]Eroare la write() spre server.\n");
               return errno;
             }
     printf("AM scris: %s \n",client.username);

    while (contor!=nrquestions)
      { 
         if (read (sd, &intrebari,sizeof(Questions)) < 0)
         {
           perror ("[client]Eroare la read() de la server.\n");
           return errno;
         }

          /* citirea mesajului */
           printf ("\n %s\n" , intrebari.question);
           printf (" %s\n",intrebari.option[0]);
           printf (" %s\n",intrebari.option[1]);
           printf (" %s\n",intrebari.option[2]);
           printf (" %s\n",intrebari.option[3]);
           fflush (stdout);
          /* scriem varianta de raspuns*/

        fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(STDIN_FILENO, &write_fds);
    FD_SET(sd, &write_fds);  //Monitorizez Socketul write
    struct timeval TimpDeRaspuns2; //trebuie sa reset timpul pt fiecare intrebare in parte
    TimpDeRaspuns2.tv_sec=TimpDeRaspuns.tv_sec;
    TimpDeRaspuns2.tv_usec=TimpDeRaspuns.tv_usec;
   
   //partea de cronometru
   timpCronometru=(int)TimpDeRaspuns2.tv_sec;
   answerDone=0; //setamm ca inca nu s a raspuns

       // Creează thread-ul de cronometru
    if (pthread_create(&threadCronometru, NULL, Cronometru, &timpCronometru) != 0) {
        printf("Eroare la crearea thread-ului de cronometru\n");  
    }


    int result = select(sd + 1,&write_fds,NULL,NULL,&TimpDeRaspuns2);

        if(result == 1){ 
           
           if(FD_ISSET(STDIN_FILENO,&write_fds))
           {
            read (STDIN_FILENO, varianta_raspuns, sizeof(varianta_raspuns));
            varianta_raspuns[strlen(varianta_raspuns)-1]='\0';
         
           /* trimiterea mesajului la server */
            printf("Ai raspuns: %s\n",varianta_raspuns);
            
             answerDone=1;
             pthread_join(threadCronometru, NULL);

           if (write (sd,&varianta_raspuns,sizeof(varianta_raspuns)) <= 0)
             {
               perror ("[client]Eroare la write() spre server.\n");
               return errno;
             }
           }
        }
        else if(result == -1){
          perror("Eroare la select!\n");
          printf("Error in result=-1 \n");
           answerDone=1;
           pthread_join(threadCronometru, NULL);
          exit(-1);
        }
        else{
            //Timpul a expirat
            printf("\nTimpul pentru răspuns a expirat.\n"); 
            pthread_join(threadCronometru, NULL);
             
        }
          contor++;
      }
  /*
         if (read (sd, &nrthreads_actualizat,sizeof(int)) <= 0)
             {
               perror ("[client]Eroare la read() din server.\n");
               return errno;
             }
        
        printf("NR_THREADS actualizati %d ,cei vechi %d\n",nrthreads_actualizat,nr.threads);
  */
         clients = malloc(sizeof(Client)*nr.threads);

        if (clients == NULL) {
        // Eroare la alocare
          printf("ERPARE la alocare");
          exit(EXIT_FAILURE);
        }
       
          if (primesteDate(sd, clients,  nr.threads) <= 0) {
            perror("Eroare la primirea datelor");
            // Gestionează eroarea
            exit(EXIT_FAILURE);
          }


        printf("\n--------------LEADER-BOARD--------------\n");

       for(int i=0; i<nr.threads; i++){ //nr.threads este cel neactualizat,cu clienti inactivi
        if(clients[i].activ==0){
          break;
        }
         printf("Locul %d: Username: %s,scor %d, secunde %ld,milisecunde %ld,microsecunde %ld \n",i+1,clients[i].username,clients[i].scor,clients[i].secunde,clients[i].milisecunde,clients[i].microsecunde);
        }

      free(clients);        
  }
   /* inchidem conexiunea, am terminat */
 
    close (sd);
}

//gcc QuizzClient.c -o QuizzClient -lpthread
//./QuizzClient 127.0.0.1 2001
//