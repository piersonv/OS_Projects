/* Client-side use of Berkeley socket calls -- send one message to server
   Requires two command line args:  
     1.  name of host to connect to (use  localhost  to connect to same host)
     2.  port number to use. 
   RAB 3/12 and Virginia Pierson 11/15*/
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAXBUFF 100

int main(int argc, char **argv) {
  char *prog = argv[0];
  char *host;
  int port;
  int sd;  /* socket descriptor */
  int ret;  /* return value from a call */
  int put_ret = 0;

  if (argc < 3) {
    printf("Usage:  %s host port\n", prog);
    return 1;
  }
  host = argv[1];
  port = atoi(argv[2]);

  if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("%s ", prog);
    perror("socket()");
    return 1;
  }
  struct hostent *hp;
  struct sockaddr_in sa;
  if ((hp = gethostbyname(host)) == NULL) {
    printf("%s ", prog);
    perror("gethostbyname()");
    return 1;
  }
  memset((char *) &sa, '\0', sizeof(sa));
  memcpy((char *) &sa.sin_addr.s_addr, hp->h_addr, hp->h_length);

  /* bzero((char *) &sa, sizeof(sa));*/
  sa.sin_family = AF_INET;
  /* bcopy((char*) sa->h_addr, (char *) &sa.sin_addr.s_addr, hp->h.length */
  sa.sin_port = htons(port);
  
  if ((ret = connect(sd, (struct sockaddr *) &sa, sizeof(sa))) < 0) {
    printf("%s ", prog);
    perror("connect()");
    return 1;
  }
  printf("Connected.\n");
  
  char *buff = NULL;  /* message buffer */
  size_t bufflen = 0;  /* current capacity of buff */
  size_t nchars;  /* number of bytes recently read */
  printf("Enter a one-line message to send (max %d chars):\n", MAXBUFF-1);
  if ((nchars = getline(&buff, &bufflen, stdin)) < 0) {
    printf("Error or end of input -- aborting\n");
    return 1;
  }
  
  //Send request header
  if ((ret = send(sd, buff, nchars-1, 0)) < 0) {
    printf("%s ", prog);
    perror("send()");
    return 1;
  }

  //Send newline after header
  char * newline = "\n";
  int n_ret = 0;
  if ((n_ret = send(sd, newline, 1, 0)) < 0) {
    printf("%s ", prog);
    perror("send()");
    return 1;
  }

  //Send file for put request
  if(strstr(buff, "PUT") != NULL){
    printf("PUT request.\n");
    //Contains PUT request
    printf("Enter file to be updated: \n");
    char * put_buff = NULL;
    size_t put_bufflen = 0;
    size_t put_chars;
    if ((put_chars = getline(&put_buff, &put_bufflen, stdin)) < 0) {
      printf("Error or end of input -- aborting\n");
      return 1;
    }

    FILE * put_file;
    char * read_buffer = NULL;
    size_t read_bufflen = 0;
    size_t read_chars;
    char * put_body;

    int i;
    //Remove ending '\n' and open file
    char file_name[strlen(put_buff)-1];
    for (i = 0; i < strlen(put_buff)-1; i++){
      file_name[i] = put_buff[i];
    }
    file_name[i] = 0; 

    put_file = fopen(file_name, "r");
    if(put_file == NULL){
      printf("Error opening file.\n");
      return 1;
    }

    //Read in from file and create body string
    while((read_chars = getline(&read_buffer, &read_bufflen, put_file)) != -1){
      asprintf(&put_body, "%s%s", put_body, read_buffer);
    }

    //Send file body to server
    if ((put_ret = send(sd, put_body, strlen(put_body), 0)) < 0) {
      printf("%s ", prog);
      perror("send()");
      return 1;
    }
  }
  int total_sent = ret + n_ret + put_ret;
  printf("(%d characters sent)\n", total_sent);

  //Receive server response
  int a, r;
  char buffer[MAXBUFF];
  while(1){
    if ((r = recv(sd, buffer, MAXBUFF-1, 0)) < 0){
      printf("recv()");
      return 1;
    }
    else{
      buffer[r] = '\0';
      printf("%s", buffer);
      if (r == 0){
        //End of file
        shutdown(sd, SHUT_RDWR);
        break;
      }
    }
  }

  //Close socket
  if ((ret = close(sd)) < 0) {
    printf("%s ", prog); 
    perror("close()");
    return 1;
  }
  return 0;
}
