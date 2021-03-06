/* client.c

   Sample code of 
   Assignment L1: Simple multi-threaded key-value server
   for the course MYY601 Operating Systems, University of Ioannina 

   (c) S. Anastasiadis, G. Kappes 2016

*/


#include "utils.h"
#include <pthread.h>

#define SERVER_PORT     6767
#define BUF_SIZE        2048
#define MAXHOSTNAMELEN  1024
#define MAX_STATION_ID   128
#define ITER_COUNT          1
#define GET_MODE           1
#define PUT_MODE           2
#define USER_MODE          3
#define THREAD_MODE        4

#define THREAD_COUNT       20

int threadsCompleted = 0;

pthread_mutex_t completed = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mWait = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cWait = PTHREAD_COND_INITIALIZER;


/**
 * @name print_usage - Prints usage information.
 * @return
 */
void print_usage() {
  fprintf(stderr, "Usage: client [OPTION]...\n\n");
  fprintf(stderr, "Available Options:\n");
  fprintf(stderr, "-h:             Print this help message.\n");
  fprintf(stderr, "-a <address>:   Specify the server address or hostname.\n");
  fprintf(stderr, "-o <operation>: Send a single operation to the server.\n");
  fprintf(stderr, "                <operation>:\n");
  fprintf(stderr, "                PUT:key:value\n");
  fprintf(stderr, "                GET:key\n");
  fprintf(stderr, "-i <count>:     Specify the number of iterations.\n");
  fprintf(stderr, "-g:             Repeatedly send GET operations.\n");
  fprintf(stderr, "-p:             Repeatedly send PUT operations.\n");
  fprintf(stderr, "-r:             Repeatedly send random operations from multiple threads.\n");
}

/**
 * @name talk - Sends a message to the server and prints the response.
 * @server_addr: The server address.
 * @buffer: A buffer that contains a message for the server.
 *
 * @return
 */
void talk(const struct sockaddr_in server_addr, char *buffer) {
  char rcv_buffer[BUF_SIZE];
  int socket_fd, numbytes;
      
  // create socket
  if ((socket_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    ERROR("socket()");
  }

  // connect to the server.
  if (connect(socket_fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) == -1) {
    ERROR("connect()");
  }
  
  // send message.
  write_str_to_socket(socket_fd, buffer, strlen(buffer));
      
  // receive results.
  printf("Result: ");
  do {
    memset(rcv_buffer, 0, BUF_SIZE);
    numbytes = read_str_from_socket(socket_fd, rcv_buffer, BUF_SIZE);
    if (numbytes != 0)
      printf("%s", rcv_buffer); // print to stdout
  } while (numbytes > 0);
  printf("\n");
      
  // close the connection to the server.
  close(socket_fd);
}


// int count = ITER_COUNT;
// char snd_buffer[BUF_SIZE];
struct sockaddr_in server_addr;
struct hostent *host_info;
int iteration;

/////////////////////
// request routine //
/////////////////////
void *req(){
  int iter = iteration;
  char snd_buffer[BUF_SIZE];
  int sta;
  int val;
  while(--iter>=0) {
      for (sta = 0; sta <= MAX_STATION_ID; sta++) {
        memset(snd_buffer, 0, BUF_SIZE);
        //  half the requests are put and half  are get
        if (sta%2) {
          // Repeatedly GET.
          sprintf(snd_buffer, "GET:station.%d", sta);
        } else{
          // Repeatedly PUT.
          // create a random value.
          val = rand() % 65 + (-20);
          sprintf(snd_buffer, "PUT:station.%d:%d", sta, val);
        }
        printf("Operation: %s\n", snd_buffer);
        talk(server_addr, snd_buffer);
      }
  }
  pthread_mutex_lock(&completed);
  threadsCompleted ++;
  if(threadsCompleted == THREAD_COUNT){
      pthread_cond_signal(&cWait);
  }
  pthread_mutex_unlock(&completed);
  return 0;
}


/**
 * @name main - The main routine.
 */
int main(int argc, char **argv) {
  int station, value;
  char *host = NULL;
  char *request = NULL;
  int mode = 0;
  int option = 0;
  char snd_buffer[BUF_SIZE];
  int count = ITER_COUNT;

  // Parse user parameters.
  while ((option = getopt(argc, argv,"i:hgpor:a:")) != -1) {
    switch (option) {
      case 'h':
        print_usage();
        exit(0);
      case 'a':
        host = optarg;
        break;
      case 'i':
        count = atoi(optarg);
	break;
      case 'g':
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -g, -p, -o, -r\n");
          exit(EXIT_FAILURE);
        }
        mode = GET_MODE;
        break;
      case 'p': 
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -g, -p, -o, -r\n");
          exit(EXIT_FAILURE);
        }
        mode = PUT_MODE;
        break;
      case 'o':
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -r, -w, -o, -r\n");
          exit(EXIT_FAILURE);
        }
        mode = USER_MODE;
        request = optarg;
        break;
      case 'r':
        if (mode) {
          fprintf(stderr, "You can only specify one of the following: -r, -w, -o, -r\n");
          exit(EXIT_FAILURE);
        }
        mode = THREAD_MODE;
        break;
      default:
        print_usage();
        exit(EXIT_FAILURE);
    }
  }

  // Check parameters.
  if (!mode) {
    fprintf(stderr, "Error: One of -g, -p, -o, -r is required.\n\n");
    print_usage();
    exit(0);
  }
  if (!host) {
    fprintf(stderr, "Error: -a <address> is required.\n\n");
    print_usage();
    exit(0);
  }
  
  // get the host (server) info
  if ((host_info = gethostbyname(host)) == NULL) { 
    ERROR("gethostbyname()"); 
  }
    
  // create socket adress of server (type, IP-adress and port number)
  bzero(&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr = *((struct in_addr*)host_info->h_addr);
  server_addr.sin_port = htons(SERVER_PORT);

  if (mode == USER_MODE) {
    memset(snd_buffer, 0, BUF_SIZE);
    strncpy(snd_buffer, request, strlen(request));
    printf("Operation: %s\n", snd_buffer);
    talk(server_addr, snd_buffer);
  } else if(mode == THREAD_MODE){ // here we do the thread mode operation
    //create multiple threads
    pthread_t tID;
    iteration = count;
    // int *pCount = &count;
    for(int i = 0; i < THREAD_COUNT; i ++){
      pthread_create(&tID, NULL, req, NULL);
    }
    // wait
    pthread_mutex_lock(&mWait);
    while(threadsCompleted <  THREAD_COUNT){
        pthread_cond_wait(&cWait, &mWait);
     }
    pthread_mutex_unlock(&mWait);

  } else { 
    while(--count>=0) {
      for (station = 0; station <= MAX_STATION_ID; station++) {
        memset(snd_buffer, 0, BUF_SIZE);
        if (mode == GET_MODE) {
          // Repeatedly GET.
          sprintf(snd_buffer, "GET:station.%d", station);
        } else if (mode == PUT_MODE) {
          // Repeatedly PUT.
          // create a random value.
          value = rand() % 65 + (-20);
          sprintf(snd_buffer, "PUT:station.%d:%d", station, value);
        }
        printf("Operation: %s\n", snd_buffer);
        talk(server_addr, snd_buffer);
      }
    }
  }
  return 0;
}




