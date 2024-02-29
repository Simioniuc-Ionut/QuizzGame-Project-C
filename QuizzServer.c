#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <stddef.h>
#include <sqlite3.h>

#include <signal.h>
#include <stdbool.h>

//#include <atomic>

/* portul folosit */
#define PORT 2001
#define MAX_CLIENTS 100
#define MAX_QUESTIONS 100

/* codul de eroare returnat de anumite apeluri */
extern int errno;



typedef struct {
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int clientSocket; //descriptorul intors de accept
}threadData;

typedef struct{
    
     int scor;
     char username[100];
     int nr_inregistrare;

     long secunde;
     long milisecunde;
     long microsecunde;

     int activ;
}Client;

typedef struct {
    //Intrebarea curenta
     char question[200];
     char option[10][200];
     char id_question[20];
     char corect[10];
}Questions;

typedef struct{
  int questions;
  int threads;
}Count;


//vector de clienti si de intrebari
Client *clients;
//Client clients[100];
Questions *intrebari; //nu e nevoie sa punem null, global = compilatorul pune default pe null
Count nr; 

    
int nrquestions;
int nrClientsFinished;
int nrthreads;
int isAlocated;
volatile int Start;

pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
pthread_t StartThread;
pthread_mutex_t mutex_lock=PTHREAD_MUTEX_INITIALIZER;  // variabila mutex ce va fi partajata de threaduri
pthread_mutex_t mutex_lock_nrthreads=PTHREAD_MUTEX_INITIALIZER;  // variabila mutex ce va fi partajata de threaduri

pthread_cond_t condStart=PTHREAD_COND_INITIALIZER;

//handler temporar
void signal_handler(int signal) {
    free(intrebari); // Eliberăm memoria alocată dinamic'
    printf("Am dealocat memoria dinamica\n");
    exit(signal); // Terminăm programul cu codul semnalului
}

//functii
void *treat(void *); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
int raspunde(void *);
void leaderboard();
void threadCreate(int);
void modificariClient(int,int,int,char *,long,long,long);
//void *QuestionFunction();
void *StartFunction(void *);
int Create_Insert_Database(const char*);
int insert_data(sqlite3 *, const char *);
void query_database(const char*);

int main()
{
    //Initializari 1
     struct sockaddr_in server;	// structura folosita de server
     struct sockaddr_in from;	
     int sd,on=1;        //descriptorul de socket 
     

     //intrebari=QuestionFunction();
     //Crearea bazei de date:
          if (Create_Insert_Database("quiz_game.db") != 0) {
          fprintf(stderr, "Failed to create database\n");
          return 1;
          }
          query_database("quiz_game.db");
          printf("Database created successfully\n");
  
    //verificam daca intrebarile s au alocat corect
       for(int i=0; i<nrquestions; i++)
       {
        printf("id: %s \nintrebare:  %s\n",intrebari[i].id_question,intrebari[i].question);
        for(int j=0; j<=3; j++){
            printf("option: %s \n",intrebari[i].option[j]);
        }
        printf("correct: %s \n",intrebari[i].corect);
       }

    //Pregatiri socket:
    if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("[server]Eroare la socket().\n");
      return errno;
    }
     /* utilizarea optiunii SO_REUSEADDR */   
     setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

     /* pregatirea structurilor de date */
     bzero (&server, sizeof (server));
     bzero (&from, sizeof (from));

     /* umplem structura folosita de server */
       server.sin_family = AF_INET;	
       server.sin_addr.s_addr = htonl (INADDR_ANY);
       server.sin_port = htons (PORT);


    /* atasam socketul */
     if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
     {
       perror ("[server]Eroare la bind().\n");
       return errno;
     }
    
    /* punem serverul sa asculte daca vin clienti sa se conecteze */
     if (listen (sd, 5) == -1)
     {
       perror ("[server]Eroare la listen().\n");
       return errno;
     }

        printf ("[server]Asteptam la portul %d...\n",PORT);
        fflush (stdout);

        pthread_create(&StartThread, NULL, StartFunction, NULL);

        /* servim in mod concurent clientii...folosind thread-uri */

        //temporar: Doar pt a elibera memoriea alocata  
        signal(SIGINT, signal_handler);

        while (1)
      {
         int client;
         int length = sizeof (from); 
         printf("start%d\n",Start);
                   
         if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
         {
    	     perror ("[server]Eroare la accept().\n");
    	     continue;
    	   }

         if(Start!=1)
         {
            threadCreate(client); 
         }
         else
         {          

            Count DejaInceput;
            DejaInceput.questions=-1;
            DejaInceput.threads=-1;
           if (write (client, &DejaInceput, sizeof(Count)) <= 0)
      		 {
      		  perror ("Clientul a intampinat o eroare imediat dupa ce a inceract sa se conecteze la server.\n");
           }
    
           close(client);  
         }

       }

    printf("FINAL\n");

    free(intrebari);
    //vreau sa ma asigur ca procesul principal nu se incheie pana cand nu se incheie toate celelalte treaduri:
    pthread_exit(NULL);    

    return 0;
 }
/*
void *QuestionFunction(){
  Questions *intrebari;
    intrebari=malloc(sizeof(Questions) * MAX_QUESTIONS);
  strcpy(intrebari[0].question,"Cat face 2 + 2 ?");
  strcpy(intrebari[0].option[0], "1");
  strcpy(intrebari[0].option[1], "3");
  strcpy(intrebari[0].option[2], "5");
  strcpy(intrebari[0].option[3], "4");
  intrebari[0].corect=4;
  strcpy(intrebari[1].question,"Care este capitala Frantei?");
  strcpy(intrebari[1].option[0], "Berlin");
  strcpy(intrebari[1].option[1], "Madrid");
  strcpy(intrebari[1].option[2], "Paris");
  strcpy(intrebari[1].option[3], "Bucuresti");
  intrebari[1].corect=3;

  return intrebari;
}*/
int insert_data(sqlite3 *db, const char *sql) {
    char *err_msg = 0;
    int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        return 1;
    }
    return 0;
}
int Create_Insert_Database(const char* Nume_BazaDeDate){

  sqlite3 *db;
  char *err_msg=0;
  int rc;

  rc = sqlite3_open(Nume_BazaDeDate, &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Nu se poate deschide baza de date: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    const char *sql = "CREATE TABLE IF NOT EXISTS Quiz ("
                      "id INTEGER PRIMARY KEY, "
                      "question TEXT NOT NULL, "
                      "option_a TEXT NOT NULL, "
                      "option_b TEXT NOT NULL, "
                      "option_c TEXT NOT NULL, "
                      "option_d TEXT NOT NULL, "
                      "correct_option CHAR(1) NOT NULL);";

  rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

   if (rc != SQLITE_OK ) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
      //inserez
    const char *sql_insert = "INSERT OR IGNORE INTO Quiz(id,question, option_a, option_b, option_c, option_d, correct_option) VALUES (1,'Care este capitala Frantei?', 'Berlin', 'Madrid', 'Paris', 'London', 'C');";
                            // "INSERT OR IGNORE INTO Quiz(id,question, option_a, option_b, option_c, option_d, correct_option) VALUES (2,'Cat face : 2+2 = ?','1','2','3','4','D');"
                           //  "INSERT OR IGNORE INTO Quiz(id,question, option_a, option_b, option_c, option_d, correct_option) VALUES (3,'Cate litere contine numele Andrei?','6','4','2','9','A');";

    if (insert_data(db, sql_insert) != 0) {
        sqlite3_close(db); 
        return 1;     
    }

    sqlite3_close(db);
    return 0;
}
void query_database(const char* db_name) {
    sqlite3 *db;
    char *err_msg = 0;
    sqlite3_stmt *stmt;
    char Row[100][100];
    char buff[100];
    int j=0;
    
    int rc = sqlite3_open(db_name, &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    const char *sql = "SELECT * FROM Quiz;";

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK ) {
        fprintf(stderr, "Failed to select data: %s\n", err_msg);
        sqlite3_free(err_msg);
    }
    printf("1 \n");
    //partea de prelucrare a fiecarei inregistrari;
     //Questions *intrebari;
    intrebari=malloc(sizeof(Questions) * MAX_QUESTIONS);

     if (intrebari == NULL) {
        // Eroare la alocare
          printf("ERPARE la alocare intrebarilor \n");
         return;
        }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int num_columns = sqlite3_column_count(stmt);
         Row[j][0] = '\0'; // Inițializează Row[j]

      for (int i = 0; i < num_columns; i++) {
        const char *col_name = sqlite3_column_name(stmt, i);
        const char *col_text = sqlite3_column_text(stmt, i);

        if (col_name) strcpy(buff, col_name);
        else strcpy(buff, "NULL");

        strcat(Row[j], buff);
        strcat(Row[j], " : ");

        if (col_text) strcpy(buff, col_text);
        else strcpy(buff, "NULL");

        strcat(Row[j], buff);
        strcat(Row[j], "\n");

          if(i == 0) //coloana id
          {
             strcpy(intrebari[j].id_question,col_text);
          }
          else if(i == 1) //coloana question
          {
            strcpy(intrebari[j].question,col_name);
            strcat(intrebari[j].question," : ");
            strcat(intrebari[j].question,col_text);
          }
           else if(i >= 2 && i <= 5) //coloane options
          {
            strcpy(intrebari[j].option[i-2],col_name);
            strcat(intrebari[j].option[i-2]," : ");
            strcat(intrebari[j].option[i-2],col_text);
          }
           else if(i == 6) //coloana correct_answer
          {
            strcpy(intrebari[j].corect,col_text);
          }
          if( i == 0)   //verific daca urm. pas este ultimul  
          {
            nrquestions = atoi(col_text);
            nr.questions= atoi(col_text);
          }                                        //obtinem numarul de intrebari:
        
        
      }
      printf("%s", Row[j]);
      j++;

    }
    printf("\nnrquestion din db = %d %d \n",nrquestions,nr.questions); 

    if (rc == SQLITE_ERROR) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

}
void modificariClient(int error ,int id_Client, int newscor,char * username,long secunde,long milisecunde,long microsecunde){

 if(error == 0) //inseamna ca nu s a produs nici o eroare
 {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         
  pthread_mutex_lock(&mutex_lock);
          /*Sectiunea Critica*/
  clients[id_Client].scor = newscor;
  clients[id_Client].secunde=secunde;
  clients[id_Client].milisecunde=milisecunde;
  clients[id_Client].microsecunde=microsecunde;
  clients[id_Client].nr_inregistrare=id_Client;
  clients[id_Client].activ=1; 
  nrClientsFinished--;
   if(nrClientsFinished==0){ 
    //apelam functia de leaderboard
    leaderboard();
    printf("nr clients finished din modificari %d\n",nrClientsFinished);
  }

  pthread_mutex_unlock(&mutex_lock);
  
 }
 else if(error == 1 ){ // s a terminat cu eroare
    pthread_mutex_lock(&mutex_lock); //folosim acelasi tip de lacat,pt a astepta in caz ca un thread verifica if ulde nrClientsFinished)
    nrClientsFinished--; //scadem din nr de clientii ,pt ca si acest client s a terminat
    if(isAlocated){//pentru cazul in care nu s a alocat inca memoria 
          clients[id_Client].activ=0;
        if(nrClientsFinished==0){ 
         //apelam functia de leaderboard
         leaderboard();

         }
    }
        printf("nr clients finished error 1 ,inainte de modificari\n  nClFinis=%d | nrthr=%d |nr.thr=%d ,sizeof(clients)=%ld\n",nrClientsFinished,nrthreads,nr.threads,sizeof(clients));
     pthread_mutex_unlock(&mutex_lock);
   
   
 }
 else if(error == 2){ //eroare dupa ce s a mai apelat modificari odata

      pthread_mutex_lock(&mutex_lock);
      printf("nr clients finished error 2 ,dupa de modificari  %d\n",nrClientsFinished);
      nrClientsFinished--;
        //aici nu am nevoie de conditia nrthreads!=0,deoarece eroarea 2 se declanseaza dupa ce s a alocat memoria pentru clienti
        clients[id_Client].activ=0;
        if(nrClientsFinished==0){ 
        //apelam functia de leaderboard
        leaderboard();
        }
      pthread_mutex_unlock(&mutex_lock);
 }

}
void threadCreate(int client){
    void *treat(void *);
    void modificariClient(int ,int , int ,char *,long,long,long);
    void *StartFunction(void *);
    void leaderboard();
      //zona de creare de thread
    threadData * td;
    pthread_mutex_lock(&mutex_lock);
    if(Start != 1)
    {
          
      td=(threadData*)malloc(sizeof(threadData));
      td->idThread=nrthreads++; //nrthreads trebuie actualizata intre lacate
	    td->clientSocket=client;
      
       if (td == NULL) {
        // Eroare la alocare
          printf("ERPARE la alocare threadului \n");
          exit(-1);
        }

     //crestem numarul de clienti din quizz
     nrClientsFinished++;
    }
    else
    {
      pthread_mutex_unlock(&mutex_lock); //deblocam lacatul inainte de a iesi din program,pt a nu ramane lacatul blocat/
       exit(-1); // daca se creaza un thread in acelasi timp cand s-a dat deja startul
    }
    pthread_mutex_unlock(&mutex_lock);

    pthread_create(&th[nrthreads], NULL, &treat, td);
}
void *treat(void * arg)
{		
		threadData tdL; 
		tdL= *(( threadData*)arg);
    int current_question_nr=0,tip_return;


		printf ("[thread]- %d - Asteptam mesajul...\n", tdL.idThread);
		fflush (stdout);		 
    tip_return=raspunde(( threadData*)arg);

        /* am terminat cu acest client, inchidem conexiunea */
    
		

    pthread_mutex_lock(&mutex_lock);

       nrthreads--; //scadem threadul folosit;

      if(nrthreads == 0 && Start!=0){
        
        //aici va intra doar ultimul thread care urmeaza sa se inchida si el
        Start=0;
        isAlocated=0; //lasam sa se reinitializeze pt noua sesiune de quizz nr.threads
        free(clients); 
       
        //nr.threads=0;//reinitializam nr de threads creati pe sesiune(acestia sunt si activi si inactivi din cadrul sesiunii)
        pthread_create(&StartThread, NULL, StartFunction, NULL); //repornim alta sesiune
      }
    pthread_mutex_unlock(&mutex_lock);

     if(tip_return == -1){
      //inseamna ca s a terminat cu eroare,si nu s a mai ajuns sa se apeleze functia de modificare client
      modificariClient(1,tdL.idThread,0,0,0,0,0);
    }
    else if(tip_return==-2){
     //inseamna ca s a terminat cu eroare dupa ce s a apelat deja functia modificari,deci trebuie sa avem grija sa nu scadem mai mult decat trebuie din nrClientFinished     
      modificariClient(2,tdL.idThread,0,0,0,0,0);
    }
   
    printf("\n>>\n" );
    //free(arg);
      
    //pthread_detach(pthread_self());	//cumva imi da eroare ,ca se intercaleaza cu free clients si nu dealoca cum trebuie// se "desprinde" thredul singur, se elibereza memoria singura.
    close(tdL.clientSocket);
    return(NULL);	
    	
};
ssize_t trimiteDate(int socket, Client *clients, int nrClients) {
    size_t lungimeClient = sizeof(Client);
    for (int i = 0; i < nrClients; i++) {
        const char *ptr = (const char *)&clients[i];
        size_t trimis = 0;

        while (trimis < lungimeClient) {
            ssize_t rezultat = write(socket, ptr + trimis, lungimeClient - trimis);
            if (rezultat <= 0) {
                //Eroare sau conexiune închisă
                printf("Eroare in write in functia de trimiteDate\n");
                return rezultat;
            }
            trimis += rezultat;
        }
    }

    return nrClients * lungimeClient;
}
int raspunde(void *arg)
{
    //declarari
   int scor=0, i=0;
   char username[100]={0},varianta_raspuns[4]={0};
	 int count={0}; 
   threadData tdL; 
	 tdL= *(( threadData*)arg);
  
   //partea de temporizator
   struct timeval TimpDeRaspuns;
   TimpDeRaspuns.tv_sec=10; //cu n secuunde
   TimpDeRaspuns.tv_usec=0;
   struct TimpTotal{
     long secunde;
     long milisecunde;
     long microsecunde;
   }; 
   bool VerifyTime=true;
   
   struct TimpTotal Total={0};
   
   //facem partea de asteptare a clientilor pt conectarea la quizz. 
   fd_set read_fds;
   struct timeval tv;
   int result={0}; 
   // Initializare setul de citire si timpul de asteptare
   FD_ZERO(&read_fds);
   FD_SET(tdL.clientSocket, &read_fds);
   tv.tv_sec = 1;  // timeout scurt, 1 secundă,pentru a permite buclei periodic sa verifice conditia din while,altfel ar sta blocat in select.
   tv.tv_usec = 0; // Dupa ce timpul a expirat result intoarce 0 ,si reverifica bucla while,dupa care iar asteapta in select.

  //verific daca da eroare inainte de a incepe quizzul
    while (Start == 0) {
      // Se actualizeaza setul de citire si timpul de asteptare
      FD_ZERO(&read_fds);
      FD_SET(tdL.clientSocket, &read_fds);

      result = select(tdL.clientSocket + 1, &read_fds, NULL, NULL, &tv);

      if (result > 0) {
          // Verifică dacă este activitate pe socket
          if (FD_ISSET(tdL.clientSocket, &read_fds)) {

              //read este folosit pt a citi daca exista activitate pe socket sau daca clientul s a deconectat,atunci read va intoarce 0. daca clientul a suferit o eroare ,va intoarce -1
              char buffer[1];  
              ssize_t bytes_read = read(tdL.clientSocket, buffer, sizeof(buffer));
              if (bytes_read <= 0) {
                  printf("Clientul s-a deconectat.\n");
                  return -1;  // Ieșire din bucla while
              }
          }
      } else if (result < 0) {
          // A apărut o eroare la select()
          perror("Eroare la select()");
          return -1;  
      }

    }

    sleep(1);

    pthread_mutex_lock(&mutex_lock_nrthreads);
    if(isAlocated==0)
    {//ne asiguram ca un singur thread va stabili valoarea nrthreads pe tot parcursul quizzului,si ca alt thread nu l va mai schimba
    nr.threads=nrthreads;
    
    clients=malloc(sizeof(Client)*nr.threads);
     
     printf("!!!S au alocat clientii!!!\n");
     if (clients == NULL) {
        // Eroare la alocare
          printf("ERPARE la alocare clientilor \n");
          return-1;
        }

    isAlocated=1;
    }
    pthread_mutex_unlock(&mutex_lock_nrthreads);

   /*scriu numarul de intrebari*/
      printf("nrquestions: %d\n",nr.questions);
   
      printf("nrthreads: %d\n",nr.threads);
   //if (write (tdL.clientSocket, &nr, sizeof(Count)) <= 0)
   if (write (tdL.clientSocket, &nr, sizeof(Count)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() catre client.\n");
      return(-1);
		}
	else
		printf ("[Thread %d]Mesajul a fost trasmis cu succes1.\n",tdL.idThread);	
  
    //trimit timpul setat catre client.
   if (write (tdL.clientSocket, &TimpDeRaspuns, sizeof(struct timeval)) <= 0)
		{
		 printf("[Thread %d] ",tdL.idThread);
		 perror ("[Thread]Eroare la write() catre client.\n");
     return(-1);
		}
	else
		printf ("[Thread %d]Mesajul a fost trasmis cu succes2.\n",tdL.idThread);	

   //inregistrare client
   if (read (tdL.clientSocket, &clients[tdL.idThread],sizeof(Client)) <= 0)
  			{
  			  printf("[Thread %d]\n",tdL.idThread);
  			  perror ("Eroare la read() de la client.\n");
          return(-1);
  			}
        printf("am citit username ul: %s\n",clients[tdL.idThread].username);



  while(i!=nrquestions)
  {
        VerifyTime = true;
        struct timeval TimpDeRaspuns,StartTimpRaspuns,EndTimpRaspuns;
       TimpDeRaspuns.tv_sec=10; //cu n secuunde
       TimpDeRaspuns.tv_usec=0;
         //setez timeout-ul pentru operatia de citire
    //il setez aici ca acum vreau sa l pun,dupa ce s au inregistrat clientii
      
          /*Verificare ,Sa vad daca inrebarile s au alocat cum trebuie din baza de date*/
        printf("R-id: %s \nR-intrebare:  %s\n",intrebari[i].id_question,intrebari[i].question);
        for(int j=0; j<=3; j++){
            printf("R-option: %s \n",intrebari[i].option[j]);
        }
        printf("R-correct: %s \n",intrebari[i].corect);

    	      /* scriem intrebarea cu variantele de raspuns clientului */

  	 if (write (tdL.clientSocket, &intrebari[i], sizeof(Questions)) <= 0)
  		{
  		 printf("[Thread %d] ",tdL.idThread);
  		 perror ("[Thread]Eroare la write() catre client.\n");
       return(-1);
  		}
  	  else
  		printf ("[Thread %d]Mesajul a fost trasmis cu succes3.\n",tdL.idThread);	

      fd_set read_fds;
     FD_ZERO(&read_fds);
     FD_SET(tdL.clientSocket, &read_fds);

      gettimeofday(&StartTimpRaspuns, NULL); //functioneaza ca un cronometru.start
      int result = select(tdL.clientSocket + 1,&read_fds,NULL,NULL,&TimpDeRaspuns);

   if(result == 1){ //daca intoarce 1,atunci s a intors varianta de raspuns
    gettimeofday(&EndTimpRaspuns, NULL); //end cronometru
     if (read (tdL.clientSocket, &varianta_raspuns,sizeof(varianta_raspuns)) <= 0)
  			{
          
  			  printf("[Thread %d]\n",tdL.idThread);
  			  perror ("Eroare la read() de la client.\n");
      
          return(-1);
        }   
      
       //calcularea timpului 
       long secunde = EndTimpRaspuns.tv_sec-StartTimpRaspuns.tv_sec;   
       long microsecunde =EndTimpRaspuns.tv_usec-StartTimpRaspuns.tv_usec;
 
    // Ajustează microsecundele dacă este necesar
      if (microsecunde < 0) {
          microsecunde += 1000000;
          secunde -= 1;
      }
        long milisecunde=microsecunde/1000;
       
       printf("Timpul de răspuns: %ld secunde %ld milisecunde %ld microsecunde\n", secunde, milisecunde,microsecunde);

    Total.secunde+=secunde;

    if(Total.milisecunde+milisecunde>=1000){
      Total.secunde+=1;
     Total.milisecunde=Total.milisecunde+milisecunde-1000; 
    }
    else{
      Total.milisecunde+=milisecunde;
    }
    if(Total.microsecunde+microsecunde>=1000000){
      Total.secunde+=1;
      Total.microsecunde=Total.microsecunde+microsecunde-1000000;  
    }
    else{
      Total.microsecunde+=microsecunde;
    }

    }
    else if(result == -1){
       gettimeofday(&EndTimpRaspuns, NULL); 
       perror("Eroare la select!\n");
          //ies
         // close(tdL.clientSocket);
          return(-1);
    }
    else{
       gettimeofday(&EndTimpRaspuns, NULL);
        //Timpul a expirat
            printf("Timpul pentru răspuns a expirat.\n");
            VerifyTime=false;
        
           //calcularea timpului 
       long secunde = EndTimpRaspuns.tv_sec-StartTimpRaspuns.tv_sec;   
       long microsecunde = EndTimpRaspuns.tv_usec-StartTimpRaspuns.tv_usec;
 
       // Ajustează microsecundele dacă este necesar
       if (microsecunde < 0) {
           microsecunde += 1000000;
           secunde -= 1;
       }   
       long milisecunde=microsecunde/1000;
       
       printf("Timpul de răspuns: %ld secunde %ld milisecunde %ld microsecunde\n", secunde, milisecunde,microsecunde);
      Total.secunde+=10;
    }

    if(VerifyTime == true)
    printf ("[Thread %d]Mesajul a fost receptionat...%s\n",tdL.idThread, varianta_raspuns);

    //verific ,nu conteaza daca e upper case sau lower case printf("varianta_raspuns: '%s', corect: '%s'\n", varianta_raspuns, intrebari[i].corect);

    if(strcasecmp(varianta_raspuns, intrebari[i].corect) ==0 && VerifyTime == true) //sa fie corect atat pt litere mari cat si mici
    {  
    
     scor++;
     printf("scor++\n"); 
    }	      /*pregatim mesajul de raspuns */    
    
    i++;
  }		     

   printf("Username: %s , intrbari %d, scor %d , secunde %ld,milisecunde %ld,microsecunde %ld\n",clients[tdL.idThread].username,i,scor,Total.secunde,Total.milisecunde,Total.microsecunde);
   
   modificariClient(0,tdL.idThread,scor,clients[tdL.idThread].username,Total.secunde,Total.milisecunde,Total.microsecunde);
     
    //trebuie sa asteptam sa termine toti participantii;

    fd_set read_fds2;
    struct timeval tv1;
    int result1={0};

    // Initializare setul de citire si timpul de asteptare
    FD_ZERO(&read_fds2);
    FD_SET(tdL.clientSocket, &read_fds2);
    tv1.tv_sec = 1;  // timeout scurt, 1 secundă,pentru a permite buclei periodic sa verifice conditia din while,altfel ar sta blocat in select.
    tv1.tv_usec = 0; // Dupa ce timpul a expirat result intoarce 0 ,si reverifica bucla while,dupa care iar asteapta in select.
    printf("Finishd%d\n",nrClientsFinished);
    while(nrClientsFinished>0){
      // Se actualizeaza setul de citire si timpul de asteptare
      FD_ZERO(&read_fds2);
      FD_SET(tdL.clientSocket, &read_fds2);

      result1 = select(tdL.clientSocket + 1, &read_fds2, NULL, NULL, &tv1);

      if (result1 > 0) {
          // Verifică dacă este activitate pe socket
          if (FD_ISSET(tdL.clientSocket, &read_fds2)) {

              //read este folosit pt a citi daca exista activitate pe socket sau daca clientul s a deconectat,atunci read va intoarce 0. daca clientul a suferit o eroare ,va intoarce -1
              char buffer[1]={0};  
              ssize_t bytes_read = read(tdL.clientSocket, buffer, sizeof(buffer));
              if (bytes_read <= 0) {
              printf("Clientul s-a deconectat.\n");
                  return -2;  // Ieșire din bucla while
              }
          }
      } else if (result1 < 0) {
          // A apărut o eroare la select()
        perror("Eroare la select()");
         return -2;  
      }
      //asteptam sa termine toti
    }


    if (trimiteDate(tdL.clientSocket, clients, nr.threads) <= 0) {
    perror("Eroare la trimiterea datelor");
    // Gestionează eroarea

      return(-2);
     }

  //functia s a finalizat cu succes
  return (1);
}
void *StartFunction(void *arg) {
    int input;
    printf("Introduceți 1 pentru a începe Quizz ul: \n");
    scanf("%d", &input);
    if (input == 1) {
       printf("Quizz ul  a început!\n"); 
       
     pthread_mutex_lock(&mutex_lock);
       
       if(nrthreads==0){
          
          printf("Nu puteti incepe fara jucatori\n");
          free(clients);
          clients = NULL;
          pthread_create(&StartThread, NULL, StartFunction, NULL); //cream iar threadul de start pt a astepta comanda
       }
       else{
        Start = 1;}
     pthread_mutex_unlock(&mutex_lock);
    }else{
        printf("Opțiune nevalidă. Serverul rămâne in asteptare de clienti.\n");
    }
    pthread_exit(NULL);
}
void leaderboard()
{
   //leaderboard este o functie ce se apeleaza in sectiunea critica,aceasta sorteaza vectorul alocat dinamic
   //daca vectorul dinamic de clienti nu a fost inca alocat,atunci nu am voie sa apelez aceasta functie
  int i=0;
  int j=0;
//Client *CopyClients = clients;


//sortam in functie de inactivitate
 //nr.threads ramane neschimbat ,adica cel vechi
for(i=0; i<nr.threads; i++){
  for(j=i+1; j<nr.threads; j++){

     int trebuieSchimbat = 0;
    if(clients[i].activ == 0) //verificam daca primmul e inactiv
    {
      if(clients[j].activ == 1){ //verificam daca si unul din urmatorii este activ,daca da,schimb
            trebuieSchimbat=1;
      }
    }
    else{
      break; //iesim din al 2 lea for,incercam cu urmatorul 
    }

    if (trebuieSchimbat) {
      printf("<< Schimbat %d cu %d >>\n",i,j);
            Client aux = clients[i];
            clients[i] = clients[j];
            clients[j] = aux;
        }
  }
}

for (i = 0; i < nrthreads; i++) {
    for (j = i + 1; j < nrthreads; j++) {
        int trebuieSchimbat = 0;
        if (clients[i].scor < clients[j].scor) {
            trebuieSchimbat = 1;
        } else if (clients[i].scor == clients[j].scor) {
            if (clients[i].secunde > clients[j].secunde) {
                trebuieSchimbat = 1;
            } else if (clients[i].secunde == clients[j].secunde) {
                if (clients[i].milisecunde > clients[j].milisecunde) {
                    trebuieSchimbat = 1;
                } else if (clients[i].milisecunde == clients[j].milisecunde) {
                    if (clients[i].microsecunde > clients[j].microsecunde) {
                        trebuieSchimbat = 1;
                    } else if (clients[i].microsecunde == clients[j].microsecunde) {
                        if (clients[i].nr_inregistrare > clients[j].nr_inregistrare) {
                            trebuieSchimbat = 1;
                        }
                    }
                }
            }
        }

        if (trebuieSchimbat) {
            Client aux = clients[i];
            clients[i] = clients[j];
            clients[j] = aux;
        }
    }


}

for (int i = 0; i < nrthreads; i++) {
    printf("Locul %d: Username: %s, scor %d, secunde %ld, milisecunde %ld, microsecunde %ld\n", 
           i + 1, clients[i].username, clients[i].scor, clients[i].secunde, 
           clients[i].milisecunde, clients[i].microsecunde);
}

}

//gcc QuizzServer.c -o QuizzServer -lsqlite3
//./QuizzServer
//cd Documents/ANUL2/TEMA2