#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAXTOKS 100

struct name{
  char** tok;
  int count;
  int status;
};

int file_length = 0;

enum status_value {NORMAL, EOF_OR_ERROR, TOO_MANY_TOKENS, SYNTAX_ERROR};

int read_name(struct name* n, char * buff){
  	n->tok = malloc(sizeof(char*)*MAXTOKS);
  
  	char* buffer = NULL;
 	size_t len = 0;
  	int ret;

  	printf("Please enter in the GET request: \n");

  	ret = getline(&buffer, &len, buff);

  	if(ret < 0 || !strcmp(buffer,"\n")){
    	return n->status = EOF_OR_ERROR;
  	}
  
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
  
  	n->count = tokNum;

  	if (strcmp(n->tok[0], "GET") || strcmp(n->tok[2],"HTTP/1.1")){
  		return n->status = SYNTAX_ERROR;
  	}
  
  	return n->status = NORMAL;
}

int main(int argc, char **argv, char **envp){  
	char buff[MAXTOKS] = "GET /myfile.txt HTTP/1.1";
  	struct name Name;
  	int ret = read_name(&Name, buff);
  	if(ret == 1)
    	printf("Error or end of file encountered.\n");
  	else if (ret == 2)
    	printf("Too many tokens entered.\n");
    else if(ret == 3)
    	printf("Syntax error found.\n");
    else{
    	FILE * file;
		char * buffer = NULL;
		size_t len = 0;

		int j = 0;
		char file_name[strlen(Name.tok[1])];
		for (int i = 1; i < strlen(Name.tok[1]); i++, j++){
			file_name[j] = Name.tok[1][i];
		}
		file_name[j] = 0;
		
		file = fopen(file_name, "r");
		if (file == NULL) 
	     	perror ("Error opening file.\n");
	   	else {
	   		while (getline(&buffer, &len, file) >= 0)
	       		fprintf(stdout, "%s", buffer);
	       	fclose(file);
	   	}
    }
}