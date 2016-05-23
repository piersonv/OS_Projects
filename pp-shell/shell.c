#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXTOKS 100

enum status_value {NORMAL, EOF_OR_ERROR, TOO_MANY_TOKENS};

struct name{
  	char** tok;
  	int count;
  	int status;
  	char* redirectIn;
  	char* redirectOut;
};

void create_process(int input, int output, char** argv){
  pid_t pid = fork();
  if(pid == 0){ //Child process
    //Check redirect in and out
    if(input != 0){
      dup2(input, 0);
      close(input);
    }
    if(output != 1){
      dup2(output, 1);
      close(output);
    }
    execvp(argv[0], argv);
  }
}

int process_pipes(struct name* pipe_n, struct name* n, int name_len){
  int current_command = 0;
  //Create pipe
  int fds[2];
  pid_t pid;
  int i;
  int in = 0;
  int out = dup(1);

  if(n->redirectIn != NULL){
    int fd = open(n->redirectIn, O_RDONLY);
    close(0);
    dup(fd);
  }

  for(i = 0; i < name_len-1; i++){
    //Create Pipe
    pipe(fds);

    create_process(in, fds[1], pipe_n[i].tok);

    //Parent doesn't need to write
    close(fds[1]); 

    in = fds[0]; //Next child will read
  }
  if(in != 0){
    close(0);
    dup(in);
  }
  
  return execvp(pipe_n[i].tok[0], pipe_n[i].tok);
}

int  read_pipes(struct name* n, int num_pipes){
  int name_len = num_pipes+1;
  struct name pipe_n[name_len];
  int current_name = 0;
  int current_tok = 0;
  
  while(current_name < name_len){
    int i = 0;
    while(current_tok < n->count && strcmp(n->tok[current_tok], "|")){
      current_tok++;
      i++;
    } 

    pipe_n[current_name].tok = malloc(sizeof(char*)*(i));
    pipe_n[current_name].count = i;

    int j = current_tok-i;
    int k = 0;
    while(j < current_tok){
      int tok_len = 0;
      while(n->tok[j][tok_len] != '\0'){
        tok_len++;
      }
      pipe_n[current_name].tok[k] = malloc(tok_len);
      strcpy(pipe_n[current_name].tok[k],n->tok[j]);
      j++;
      k++;
    }
    pipe_n[current_name].tok[k] = 0;
    current_tok++; //Skip | character
    current_name++;
  }

  return process_pipes(pipe_n, n, name_len);
}

int redirect_IO(struct name* n, char* buffer, int numChars, int ret, int type){
	int i = 0;
	while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
      	numChars++;
    }	
    while(!isspace(buffer[numChars]) && numChars < ret){
      	numChars++;
      	i++;
    }
    if(type == 0){
    	n->redirectIn = malloc(i);
		  int j = numChars-i;
   		int k = 0;
    	while(j < numChars){
      		n->redirectIn[k] = buffer[j];
      		j++;
      		k++;
    	}
    	n->redirectIn[k] = 0;
    }
    else{
    	n->redirectOut = malloc(i);
		  int j = numChars-i;
   		int k = 0;
    	while(j < numChars){
      		n->redirectOut[k] = buffer[j];
      		j++;
      		k++;
    	}
    	n->redirectOut[k] = 0;
    }
    while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
      	numChars++;
    }
    return numChars;
}

void process_command(struct name* n){

  	//Fork
  	pid_t pid = fork();

  	if(pid == 0){ //Child process
  		//Check redirect in and out
  		if(n->redirectIn != NULL){
  			int fd = open(n->redirectIn, O_RDONLY);
  			close(0);
  			dup(fd);
  		}
  		if(n->redirectOut != NULL){
  			int fd = open(n->redirectOut, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  			if (fd < 0){
  				printf("Error opening file %s", n->redirectOut);
  			}
  			close(1);
  			dup(fd);
  		}
  		//Execute command
  		int ret = execvp(n->tok[0], n->tok);
  		if(ret < 0){
  			printf("Command '%s' not found.\n", n->tok[0]);
  		}
  	}
  	else{
  		if(strcmp(n->tok[n->count-1], "&")){
  			int status;
  			pid_t w = wait(&status);
  		}
  	}
}

int read_name(struct name* n){
	int run = 1;
	while (run){
		n->tok = malloc(sizeof(char*)*MAXTOKS);
  
  		char* buffer = NULL;
  		size_t len = 0;
  		int ret;

  		printf("virginia's-shell$ ");

  		ret = getline(&buffer, &len, stdin);

  		if(ret < 0){
  			return n->status = EOF_OR_ERROR;
  		}
      if(!strcmp(buffer, "\n")){
        continue;
      }
  
  		int numChars = 0;
  		int tokNum = 0;
  		n->redirectIn = n->redirectOut = NULL;
      int pipes = 0;

  		//Remove any leading whitespaces
  		while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
    		numChars++;
  		}		

  		while(numChars < ret){
    		int i = 0;
    
    		while(!isspace(buffer[numChars]) && numChars < ret){
      			numChars++;
      			i++;
    		}

    		char * token = malloc(i);
        int j = numChars-i;
    		int k = 0;
    		while(j < numChars){
      			token[k] = buffer[j];
      			j++;
      			k++;
    		}
    		token[k] = 0;
			
			//Check for redirection
    		if(!strcmp(token, "<")){
    			numChars = redirect_IO(n, buffer, numChars, ret, 0);
    			continue;
    		}
    		if(!strcmp(token, ">")){
    			numChars = redirect_IO(n, buffer, numChars, ret, 1);
    			continue;
    		}
        if(!strcmp(token, "|")){
          pipes++;
        }

    		n->tok[tokNum] = malloc(sizeof(char)*(i));

    		strcpy(n->tok[tokNum], token);

    		//Remove any additional whitespaces
    		while((isspace(buffer[numChars]) || buffer[numChars] == '\0') && numChars < ret){
      			numChars++;
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
  		n->tok[tokNum] = 0;

  		if(!strcmp(n->tok[0], "cd")){
        //Change directory
  			chdir(n->tok[1]);
  		}
      else if(pipes > 0){
        //Process pipe commands
        read_pipes(n, pipes);
      }
  		else if(!strcmp(n->tok[0], "exit")){
        //Exit shell
		    break;
  		}
  		else{
  			process_command(n);
  		}

      int f;
      for (f = 0; f < n->count; f++){
        free(n->tok[f]);
      }
	}
  
  	return n->status = NORMAL;
}

int main(int argc, char **argv, char **envp){  
  	struct name Name;
 	int ret = read_name(&Name);
  	if(ret == 1)
  	  	printf("Error or end of file encountered.\n");
  	else if (ret == 2)
  	  	printf("Too many tokens entered\n.");
}