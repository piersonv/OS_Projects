/* Server-side use of Berkeley socket calls -- receive one message and print 
   Requires one command line arg:  
     1.  port number to use (on this machine). 
   RAB 3/12 and Virginia Pierson 11/15 */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <pthread.h> 

#define MAXBUFF 100
#define MAXTOKS 100
#define NUMWORKERS 2

//Parsed request
struct name{
  char** tok;
  int count;
  int status;
};
//Results of parsing
enum status_value {NORMAL, EOF_OR_ERROR, TOO_MANY_TOKENS, SYNTAX_ERROR, NOT_IMPLEMENTED};

//Request information
struct tdata{
  int clientd;
  FILE * fp;
  char* prog;
  int request_id;
  struct tdata * link;
  int end;
};

//Queue of requests
struct work_queue{
  struct tdata * start;
  struct tdata ** end;
} work_queue;

//Response heaer
struct header{
  char * lines[6];
};

//Thread variables
pthread_mutex_t mutex;
pthread_cond_t empty;

//Server loop variable
int run = 1;

//Parse HTTP request and indicate any issues with syntax or request implementation
int read_name(struct name* n, char * buffer, int ret){
  //Allocate tok array
	n->tok = malloc(sizeof(char*)*MAXTOKS);

	int numChars = 0;
	int tokNum = 0;
	while(numChars < ret){
  	int i = 0;

  	//Remove any leading whitespaces
  	while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
  	  numChars++;
  	}

  	while(!isspace(buffer[numChars]) && numChars < ret){
  	  numChars++;
  	  i++;
  	}

    //Allocate and copy over array for token
  	n->tok[tokNum] = malloc(sizeof(char)*(i+1));
  	int j = numChars-i;
  	int k = 0;
  	while(j < numChars){
  	  n->tok[tokNum][k] = buffer[j];
  	  j++;
  	  k++;
  	}
  	n->tok[tokNum][k] = 0;

  	//Remove any additional whitespaces
  	while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
  	  numChars++;
  	  i++;
  	}

  	//Increment tokNum
  	tokNum++;
    //If tokNum is over MAXTOKS,
    //Add the rest of the buffer and return error
  	if (tokNum == MAXTOKS-1 && numChars != ret){
  	  n->tok[tokNum] = malloc(sizeof(char)*(ret-numChars));
  	  int l = numChars;
  	  int m = 0;
  	  while (l < ret){
  	n->tok[tokNum][m] = buffer[l];
  	l++;
  	m++;
  	  }
  	  return n->status = TOO_MANY_TOKENS;
  	}
	}
  
  //Update count
	n->count = tokNum;

  //Check for unimplemented HTTP requests
  if(!strcmp(n->tok[0], "HEAD") || !strcmp(n->tok[0], "POST") || !strcmp(n->tok[0], "DELETE") || !strcmp(n->tok[0], "TRACE") || !strcmp(n->tok[0], "OPTIONS") || !strcmp(n->tok[0], "CONNECT") || !strcmp(n->tok[0], "PATCH")){
    return n->status = NOT_IMPLEMENTED;
  }
  if(!strcmp(n->tok[0], "GET")){
    if(n->tok[1][0] != '/' || strcmp(n->tok[2],"HTTP/1.1")){
      return n->status = SYNTAX_ERROR;
    }
    return n->status = NORMAL;
  }
  //Check for syntax errors
	else if (!strcmp(n->tok[0], "PUT")){
    if(n->tok[1][0] != '/' || strcmp(n->tok[2],"HTTP/1.1")){
      printf("PUT syntax error.\n");
      return n->status = SYNTAX_ERROR;
    }
    return n->status = NORMAL;
	}
  if(strcmp(n->tok[0], "GET")){
    printf("Overall syntax error: %s.\n", n->tok[0]);
    return n->status = SYNTAX_ERROR;
  }
  if(strcmp(n->tok[0], "PUT")){
    printf("Overall syntax error2\n");
    return n->status = SYNTAX_ERROR;
  }

	return n->status = NORMAL;
}

//Add HTTP request to queue for future completion
void addQueue(struct tdata * tdata_p){
  pthread_mutex_lock(&mutex);
  if(work_queue.end == &work_queue.start){
    pthread_cond_broadcast(&empty);
  }
  *work_queue.end = tdata_p;
  tdata_p->link = 0;
  work_queue.end = &tdata_p->link;
  pthread_mutex_unlock(&mutex);
  return;
}

//Remove HTTP request from queue for thread to complete
struct tdata * removeQueue(){
  pthread_mutex_lock(&mutex);
  while (work_queue.start == 0){
    pthread_cond_wait(&empty, &mutex);
  }
  struct tdata * temp = work_queue.start;
  work_queue.start = temp->link;
  if(work_queue.start == 0){
    work_queue.end = &work_queue.start;
  }
  pthread_mutex_unlock(&mutex);
  temp->link = 0;
  return temp;
}

//When user enters 'exit' into terminal, prepare to close down server
void * set_done(void * rand){
  char * read_line = NULL;
  size_t len, r;
  r = getline(&read_line, &len, stdin);
  if(r < 0)
    printf("not working.\n");
  if (!strcmp(read_line, "exit\n")){
      printf("Server is exiting after next request.\n");
      run = 0;
  }
}

/* 
process_GET 

Received information from process_requests to fufill a GET request

Arguments:
Name - A name struct that holds the parsed request
Header - Array containing header to be sent back to the client
data - A tdata pointer to the information needed for the request
timestamp - A C array that contains the time the request was received

State Change:
The requested file is opened, the header created and sent to client with the file, and then the client connected is shutdown
*/
void process_GET(struct name Name, struct header Header, struct tdata * data, char * timestamp){
  //Open requested file
  FILE * file;
  char * buffer = NULL;
  size_t len = 0;
  size_t nChars;

  int j = 0;
  //Remove leading '/' and open file
  char file_name[strlen(Name.tok[1])-1];
  for (int i = 1; i < strlen(Name.tok[1]); i++, j++){
    file_name[j] = Name.tok[1][i];
  }
  file_name[j] = 0; 
  file = fopen(file_name, "r");
  
  if (file == NULL){
    perror ("Error opening file.\n");
    //Create 404 Not Found Header
    asprintf(&Header.lines[0], "%s 404 Not Found\r\n", Name.tok[2]);
    asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
    Header.lines[2] = "Connection: close\r\n";
    Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
    asprintf(&Header.lines[4], "Content-Length: 0\r\n");
    Header.lines[5] = "\r\n";

    //Send Header
    for(int i = 0; i < 6; i++){
      int j;
      if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
        printf("send(): %s", strerror(errno));
      } 
    }

    //Add log file entry of header response
    fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

    //Shutdown connection
    shutdown(data->clientd, SHUT_RDWR);
  } 
  else {
    //Find length of file
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    //Create 200 OKHeader
    asprintf(&Header.lines[0], "%s 200 OK\r\n", Name.tok[2]);
    asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
    Header.lines[2] = "Connection: close\r\n";
    Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
    asprintf(&Header.lines[4], "Content-Length: %d\r\n", file_size);
    Header.lines[5] = "\r\n";

    //Send Header
    for(int i = 0; i < 6; i++){
      int j;
      if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
        printf("send(): %s", strerror(errno));
      } 
    }

    //Send requested file
    while (nChars = getline(&buffer, &len, file) >= 0){
      int s;
      if ((s = send(data->clientd, buffer, strlen(buffer), 0)) < 0){
        printf("send(): %s", strerror(errno));
      }          
    }
    fclose(file);

    //Add log file entry of header response
    fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

    //Shutdown connection
    shutdown(data->clientd, SHUT_RDWR);
  }
}

/* 
process_PUT 

Received information from process_requests to fufill a PUT request

Arguments:
Name - A name struct that holds the parsed request
Header - Array containing header to be sent back to the client
data - A tdata pointer to the information needed for the request
timestamp - A C array that contains the time the request was received
put_data - A C array containing the contents of the file to be updated/created

State Change:
The requested file is opened or created and written to, the header created and sent to client, and then the client connected is shutdown
*/
void process_PUT(struct name Name, struct header Header, struct tdata * data, char * timestamp, char * put_data){
  //Open requested file
  FILE * file;
  char * buffer = NULL;
  size_t len = 0;
  size_t nChars;

  int j = 0;
  //Remove leading '/' and open file
  char file_name[strlen(Name.tok[1])-1];
  for (int i = 1; i < strlen(Name.tok[1]); i++, j++){
    file_name[j] = Name.tok[1][i];
  }
  file_name[j] = 0; 

  file = fopen(file_name, "w");
  fprintf(file, "%s", put_data);
  
  if (file == NULL){
    perror ("Error opening file.\n");
    //Create 500 Internal Server Error Header
    asprintf(&Header.lines[0], "%s 500 Internal Server Error\r\n", Name.tok[2]);
    asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
    Header.lines[2] = "Connection: close\r\n";
    Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
    asprintf(&Header.lines[4], "Content-Length: 0\r\n");
    Header.lines[5] = "\r\n";

    //Send Header
    for(int i = 0; i < 6; i++){
      int j;
      if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
        printf("send(): %s", strerror(errno));
      } 
    }

    //Add log file entry of header response
    fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

    //Shutdown connection
    shutdown(data->clientd, SHUT_RDWR);
  } 
  else {
    //Create 200 OKHeader
    asprintf(&Header.lines[0], "%s 200 OK\r\n", Name.tok[2]);
    asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
    Header.lines[2] = "Connection: close\r\n";
    Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
    asprintf(&Header.lines[4], "Content-Length: 0");
    Header.lines[5] = "\r\n";

    //Send Header
    for(int i = 0; i < 6; i++){
      int j;
      if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
        printf("send(): %s", strerror(errno));
      } 
    }

    //Add log file entry of header response
    fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

    fclose(file);

    //Shutdown connection
    shutdown(data->clientd, SHUT_RDWR);
  }
}

/* 
process_requests 

Receive HTTP request from client and sends it to process_GET or process_PUT to be fufilled

Arguments:
id - A void * that is used to identify the thread

State Change:
A accepted connection is removed from the queue, the request is received, the requests is parsed and return interpreted,
implemented and correct requests are sent on to be fufilled, and the client is closed
*/
void * process_requests(void * id){
  while(1){
    //Remove a HTTP request from the queue and process it
    int thread_id = *((int *)id);
    char * buff[20];  /* message buffer */
    int ret;  /* return value from a call */
    struct header Header;
    char put_body[MAXBUFF];

    for(int i =0; i < 20; i++){
      buff[i]=(char*)malloc(MAXTOKS*sizeof(char));
    }

    //Try to remove a request from the queue
    struct tdata *data = removeQueue();
    if(data->end == 1){
      //Break out of loop
      struct tdata * endData = (struct tdata *)malloc(sizeof(struct tdata));
      endData->end = 1;
      addQueue(endData);
      break;
    }

    //Receive the request
    int recv_line = 0;
    for(int i =0; i < 20; i++){
      if ((ret = recv(data->clientd, buff[recv_line], MAXBUFF-1, 0)) < 0) {
        printf("%s ", data->prog);
        perror("recv()");
        return NULL;
      }
      buff[recv_line][ret] = '\0';  // add terminating nullbyte to received array of char

      if(strcmp(buff[recv_line], "\n")){
        break;
      }
    }

    if(strstr(buff[0], "PUT") != NULL){
      printf("PUT request.\n");
      char extra_newline[MAXBUFF];
      if ((ret = recv(data->clientd, extra_newline, MAXBUFF-1, 0)) < 0) {
        printf("%s ", data->prog);
        perror("recv()");
        return NULL;
      }
      if ((ret = recv(data->clientd, put_body, MAXBUFF-1, 0)) < 0) {
        printf("%s ", data->prog);
        perror("recv()");
        return NULL;
      }
      put_body[ret] = '\0';  // add terminating nullbyte to received array of char
    }
    
    //Get the current time
    time_t now = time(NULL);
    char timestamp[30];
    int numBytes = strftime(timestamp, 30, "%a, %d %b %Y %T %Z", gmtime(&now));
    if (numBytes == 0){
      perror("Time over 30 chars.");
    }
    else{
      //Add the log file entries for the received request and the time
      fprintf(data->fp, "%d %d %s\n%d %s\n", data->request_id, thread_id, timestamp, data->request_id, buff[0]);
    }

    //Parse the request
    struct name Name;
    int r = strlen(buff[0]);
    int retr = read_name(&Name, buff[0], r);

    //Interpret results of parsing
    if(retr == 1){ 
      printf("Error or end of file encountered.\n");
      //Create 500 Header
      asprintf(&Header.lines[0], "%s 500 Internal Server Error\r\n", Name.tok[2]);
      asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
      Header.lines[2] = "Connection: close\r\n";
      Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
      asprintf(&Header.lines[4], "Content-Length: 0\r\n");
      Header.lines[5] = "\r\n";

      //Send 500 Header
      for(int i = 0; i < 6; i++){
        int j;
        if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
          printf("send(): %s", strerror(errno));
        } 
      }

      //Add log file entry of header response
      fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

      //Shutdown connection
      shutdown(data->clientd, SHUT_RDWR);
    }
    else if(retr == 3 || retr == 2){
      if(retr == 2){
        printf("Too many tokens received.\n");
      }
      if(retr == 3){
        printf("Syntax error found.\n");
      }

      //Create 400 Header
      asprintf(&Header.lines[0], "%s 400 Bad Request\r\n", Name.tok[2]);
      asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
      Header.lines[2] = "Connection: close\r\n";
      Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
      asprintf(&Header.lines[4], "Content-Length: 0\r\n");
      Header.lines[5] = "\r\n";

      //Send 400 Header
      for(int i = 0; i < 6; i++){
        int j;
        if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
          printf("send(): %s", strerror(errno));
        } 
      }

      //Add log file entry of header response
      fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

      //Shutdown connection
      shutdown(data->clientd, SHUT_RDWR);
    }
    else if(retr == 4){
      printf("Request not implemented.\n");
      //Create 501 Header
      asprintf(&Header.lines[0], "%s 501 Not Implemented\r\n", Name.tok[2]);
      asprintf(&Header.lines[1], "Date: %s\r\n", timestamp);
      Header.lines[2] = "Connection: close\r\n";
      Header.lines[3] = "Content-Type: test/html; charset=utf-8\r\n";
      asprintf(&Header.lines[4], "Content-Length: 0\r\n");
      Header.lines[5] = "\r\n";

      //Send Header
      for(int i = 0; i < 6; i++){
        int j;
        if ((j = send(data->clientd, Header.lines[i], strlen(Header.lines[i]), 0)) < 0){
          printf("send(): %s", strerror(errno));
        } 
      }

      //Add log file entry of header response
      fprintf(data->fp, "%d %s\n", data->request_id, Header.lines[0]);

      //Shutdown connection
      shutdown(data->clientd, SHUT_RDWR);
    }
    else{ //No issues with parsing or HTTP request
      if(!strcmp(Name.tok[0], "GET")){
        process_GET(Name, Header, data, timestamp);
      }
      if(!strcmp(Name.tok[0], "PUT")){
        process_PUT(Name, Header, data, timestamp, put_body);
      }
    }
    
    //Close connection
    if ((ret = close(data->clientd)) < 0) {
      printf("%s ", data->prog);
      perror("close(clientd)");
    }
  }
  return NULL;
}

/* 
main 

Creates socket and accepts incoming HTTP requests to be fufilled

Arguments:
argc - Number of commandline arguments
argv - Array containing the commandline arguments

State Change:
The socket is created and bound, threads are created, and then requests are accepted and added to the work_queue until the user chooses to exit
the server, the threads are joined and finally the socked is closed 
*/
int main(int argc, char **argv) {
  work_queue.start = 0;
  work_queue.end = &work_queue.start;
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&empty, NULL);
  FILE * logFile = fopen("log.txt", "w");
  if(logFile == NULL){
    printf("Error opening log file.\n");
  }
  char * prog = argv[0];
  int port;
  int serverd;  /* socket descriptor for receiving new connections */
  int ret;
  int request_id = 0;

  //Not enough arguments
  if (argc < 2) {
    printf("Usage:  %s port\n", prog);
    return 1;
  }
  port = atoi(argv[1]);

  //Open socket
  if ((serverd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  
  struct sockaddr_in sa;
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  sa.sin_addr.s_addr = INADDR_ANY;

  //Bind socket
  if (bind(serverd, (struct sockaddr*) &sa, sizeof(sa)) < 0) {
    printf("%s ", prog);
    perror("bind()");
    return 1;
  }
  //Listen on socket
  if (listen(serverd, 5) < 0) {
    printf("%s ", prog);
    perror("listen()");
    return 1;
  }

  //Create client socket address and threads variables
  struct sockaddr_in ca;
  int size = sizeof(struct sockaddr);
  pthread_t tHandles[NUMWORKERS];
  pthread_t exitHandle; 
  int id[NUMWORKERS];
  int exit_id = -1;
  for(int i = 0; i < NUMWORKERS; i++){
    id[i] = i;
  }

  //Create threads
  for(int i =0; i < NUMWORKERS; i++){
    pthread_create(&tHandles[i], NULL, process_requests, (void*)&id[i]);
  }
  pthread_create(&exitHandle, NULL, set_done, (void*)&exit_id);

  //Accept incoming connections to be processed by threads
  while(run){
    struct tdata * data;

    printf("Waiting for a incoming connection...\n");
    int client;
    if ((client = accept(serverd, (struct sockaddr*) &ca, &size)) < 0) {
      printf("%s ", prog);
      perror("accept()");
      return 1;
    }
    printf("Connection accepted.\n");

    data = (struct tdata *)malloc(sizeof(struct tdata));
    data->fp = logFile;
    data->prog = argv[0];
    data->request_id = request_id;
    data->clientd = client;
    data->end = 0;

    //Add HTTP request to queue
    addQueue(data);
    request_id++;
  }

  struct tdata * endData = (struct tdata *)malloc(sizeof(struct tdata));
  endData->end = 1;
  addQueue(endData);

  //Join threads
  for(int i =0; i < NUMWORKERS; i++){
    pthread_join(tHandles[i], NULL);
  }
  pthread_join(exitHandle, NULL);

  //Clost log file
  fclose(logFile);

  //Close socket
  if ((ret = close(serverd)) < 0) {
    printf("%s ", prog);
    perror("close(serverd)");
    return 1;
  }

  return 0;
}
