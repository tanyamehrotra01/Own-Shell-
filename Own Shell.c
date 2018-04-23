#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <wordexp.h>

//declaring the builtin function names
char *builtin_str[] = {"cd","help","exit"};

int sst_cd(char **args);
int sst_help(char **args);
int sst_exit(char **args);
int sst_execute(char **args);
int sst_launch(char **args);
char *sst_read_line(void);
char **sst_split_line(char *line, char *s);
int checkForCommands(char *line);
struct node* newNode(char *data);
void historyPrint();
void aliasFunc(char * line);
char* checkAlias(char *line);
int checkForCommands(char*);
void editor();
void executeCommandsFromEditor(char*);
void cat2Function(char *line);
void checkIFSyntax(char *line);
void printZeroSizeFiles();
void sortWithINodeTime();
void pipeInputOutput(char *line);
void pipeAndOutput(char * line);
void pipeAndInput(char * line);
void onlyRedirection(char *line);
void readFromInputFile(char *line);
void readFromOutputFile(char *line);
void parsePipedInput(char **token);
void printFilesWithRegex(char *regex);

int pipeInInputFlag = 0;
int flag = 0;
int totalNodes = 0;
int redirectionLessThan = 0;
int redirectionGreaterThan = 0;
int starFlag = 0;
int aliasArrayCount = 0;

struct alias
{
  char *originalValue;
  char *newValue;
};
struct alias *aliasArray ; //array of structures

//For history
struct node  
{
      char* data;
      char* timestamp;
      struct node *next;
      struct node *prev;
}*head,*tail;

//getting the size of the builtin funtion array
int sst_num_builtins() 
{
      return sizeof(builtin_str) / sizeof(char *);
}

int sst_cd(char **args)
{
      if (args[1] == NULL) 
      {
            fprintf(stderr, "sst: expected argument to \"cd\"\n");
      } 
      else 
      {
            if (chdir(args[1]) != 0) 
            {
                  perror("sst");
            }
      }
      return 1;
}

int sst_help(char **args)
{
      int i;
      printf("Welcome to the help page\n");
      printf("Enter the query\n");
      printf("The following functions are built in:\n");

      for (i = 0; i < sst_num_builtins(); i++) 
      {
            printf("  %s\n", builtin_str[i]);
      }

      printf("Use the man command for information on other programs.\n");
      return 1;
}

int sst_exit(char **args)
{
      printf("Bye-bye\n");
      return 0;
}

int sst_execute(char **args)
{
      int i;

      if (args[0] == NULL) 
      {
            return 1;
      }

      for (i = 0; i < sst_num_builtins(); i++) 
      {
            if (strcmp(args[0], builtin_str[i]) == 0)
            {
                  if(i == 0)
                        return sst_cd(args);
                  else if(i == 1)
                        return sst_help(args);
                  else if(i == 2)
                        return sst_exit(args);
            }
      }
      return sst_launch(args);
}

int sst_launch(char **args)
{
      pid_t pid, wpid;
      int status;
      int i = 0;
      int backgroundFlag = 0;
      while(args[i+1] != NULL)
      {
            i++;
      }
      if(strcmp(args[i],"&") == 0)
      {
            backgroundFlag = 1;
            args[i] = NULL; //removing & from BG Process
      }
      pid = fork();
      if (pid == 0) //Child Process
      {
            if (execvp(args[0], args) == -1) 
            {
                  perror("sst");
            }
            exit(EXIT_FAILURE);
      } 
      else if (pid < 0) 
      {
            perror("sst");
      } 
      else 
      {
            if(backgroundFlag != 1)
            {
                  do //wait for child to finish
                  {
                        wpid = waitpid(pid, &status, WUNTRACED);
                        /*WUNTRACED The status of any child processes specified by pid that are stopped, 
                        and whose status has not yet been reported since they stopped, shall also be 
                        reported to the requesting process.*/
                   }while (!WIFEXITED(status) && !WIFSIGNALED(status));
            }
      }
      backgroundFlag = 0;
      return 1;
}

#define SST_RL_BUFSIZE 1024

char *sst_read_line(void)
{
      int bufsize = SST_RL_BUFSIZE;
      int position = 0;
      char *buffer = malloc(sizeof(char) * bufsize);
      int c;

      if (!buffer) 
      {
            fprintf(stderr, "sst: allocation error\n");
            exit(EXIT_FAILURE);
      }

      while (1) 
      {
            // Read a character
            c = getchar();

            // If we hit EOF, replace it with a null character and return.
            if (c == EOF || c == '\n') 
            {
                  buffer[position] = '\0';
                  return buffer;
            } 
            else 
            {
                  buffer[position] = c;
            }
            position++;

            // If we have exceeded the buffer, reallocate.
            if (position >= bufsize) 
            {
                  bufsize += SST_RL_BUFSIZE;
                  buffer = realloc(buffer, bufsize);
                  if (!buffer) 
                  {
                        fprintf(stderr, "sst: allocation error\n");
                        exit(EXIT_FAILURE);
                  }
            }
      }
}

#define SST_TOK_BUFSIZE 64


char **sst_split_line(char *line, char *s)
{
      int bufsize = SST_TOK_BUFSIZE, position = 0;
      char **tokens = malloc(bufsize * sizeof(char*));
      char *token;

      if (!tokens)
      {
            fprintf(stderr, "sst: allocation error\n");
            exit(EXIT_FAILURE);
      }
      //tokenizing the string
      token = strtok(line, s);

      while (token != NULL) 
      {
            tokens[position] = token;
            position++;
            if(strcmp(token,"|") == 0) //Assuming there are spaces between pipes
            {
                  pipeInInputFlag = 1;
            }
            if(strcmp(token,"<") == 0)
            {
                  redirectionLessThan = 1;
            }
            if(strcmp(token,">") == 0)
            {
                  redirectionGreaterThan = 1;
            }
            if (position >= bufsize) 
            {
                  bufsize += SST_TOK_BUFSIZE;
                  tokens = realloc(tokens, bufsize * sizeof(char*));
                  if (!tokens)
                  {
                        fprintf(stderr, "sst: allocation error\n");
                        exit(EXIT_FAILURE);
                  }
            }
            //get next token
            token = strtok(NULL, s);
      }
      tokens[position] = NULL;
      return tokens;
}

int checkForCommands(char *line)
{
      int status = 1;
      char **args;
      char *copyLine = malloc(sizeof(char)*strlen(line));
      strcpy(copyLine,line);

      if(strcmp(line,"history") == 0)
      {
            historyPrint();
            status=1;
      }
      else if(strncmp(copyLine,"alias",5)==0)
      {
            aliasFunc(copyLine);
            status = 1;
      }
      else if(strcmp(copyLine,"shell editor")==0)
      {
            editor();
            status = 1;
      }
      else if(strncmp(copyLine,"cat2",4) == 0)
      {
            cat2Function(copyLine);
            status = 1;
      }
      else if(strncmp(copyLine,"if",2)==0)
      {
            checkIFSyntax(copyLine);
            status = 1;
      }
      else if(strcmp(copyLine,"ls -z") == 0)
      {
            printZeroSizeFiles();
            status = 1;
      }
      else if(strcmp(copyLine, "ls -itime") == 0)
      {
            sortWithINodeTime();
            status = 1;
      }
      else 
      {
            char *check = malloc(sizeof(char)*strlen(line));
            check = checkAlias(line);
            if(check != NULL)
            {
                  args = sst_split_line(check," \t\r\n\a");
            }
            else
            {
                  args = sst_split_line(line," \t\r\n\a");
            }
            if(redirectionGreaterThan == 1 && redirectionLessThan == 1 && pipeInInputFlag == 1)
            {
                  pipeInputOutput(copyLine);
            }
            else if(redirectionGreaterThan == 1 && pipeInInputFlag == 1)
            {
                  pipeAndOutput(copyLine);
            }
            else if(redirectionLessThan == 1 && pipeInInputFlag == 1)
            {
                  pipeAndInput(copyLine);
            }
            else if(redirectionLessThan == 1 && redirectionGreaterThan == 1)
            {
                  onlyRedirection(copyLine);
            }
            else if(redirectionLessThan == 1)
            {
                  readFromInputFile(copyLine);
            }
            else if(redirectionGreaterThan == 1)
            {
                  readFromOutputFile(copyLine);
            }
            else if(pipeInInputFlag == 1)
            {
                  char **token;
                  token = sst_split_line(copyLine,"|");
                  parsePipedInput(token);
                  status=1;
            }
            else
            {
                  int index = 0;
                  while(copyLine[index] != '\0')
                  {
                        if(copyLine[index] == '*')
                        {
                              starFlag = 1;
                              break;
                        }
                        index++;
                  }
                  if(starFlag != 1)
                  {
                        status = sst_execute(args);
                  }
                  else
                  {
                        char **tokens = sst_split_line(copyLine, " ");
                        char *regex = tokens[1];
                        printFilesWithRegex(regex);
                        starFlag = 0;
                        status = 1;
                  }
            }     
      }
      return status;
}

struct node* newNode(char *data)
{
      struct node *temp = malloc(sizeof(struct node));
      temp -> data = (char*)malloc(sizeof(char)*strlen(data));
      strcpy(temp->data, data);
      temp -> prev = NULL;
      temp -> next = NULL;
      return temp;
}

void historyPrint()
{
      int i;
      struct node *temp = head;
      while(temp != NULL)
      {
            printf("%s     %s      \n",temp->data,temp->timestamp);
            temp = temp->next;
      }
}

void aliasFunc(char * line)
{
      char **token;
      char **token1;
      token = sst_split_line(line,"=\"");
      token1 = sst_split_line(token[0]," ");
      aliasArray[aliasArrayCount].originalValue = malloc(sizeof(char)*strlen(token[1]));
      strcpy(aliasArray[aliasArrayCount]. originalValue,token[1]);
      aliasArray[aliasArrayCount].newValue = malloc(sizeof(char)*strlen(token1[1]));
      strcpy(aliasArray[aliasArrayCount].newValue,token1[1]);
      aliasArrayCount++;
}

char* checkAlias(char *line)
{
      int i;
      int flag = 0;
      for(i = 0 ; i < aliasArrayCount ; i++)
      {
            if(strcmp(line,aliasArray[i].newValue) == 0)
            {
                  flag = 1;
                  break;
            }
      }
      if(flag == 1)
      {
            return aliasArray[i].originalValue;
      }
      else
      {
            return NULL;
      }
}

void editor()
{
      int i = 0;
      pid_t x = fork();

      if(x==0)
      {
            char *buf = malloc(sizeof(char)*100);
            int endFlag = 1; //for \q
            int enterFlag = 0; //for \
            
            while(endFlag)
            {
                  char c = getchar();
                  if(enterFlag && c == 'q')
                  {
                        endFlag = 0;
                        buf[i] = '\0';
                        executeCommandsFromEditor(buf);
                        _exit(1);
                  }
                  else
                  {
                        enterFlag = 0;
                  }
                  if(c == '\\')
                  {
                        enterFlag = 1;
                  }
                  if(c != '\n')
                  {
                        buf[i++] = c;
                  }
            }
      }
      else
      {
            wait(NULL);
      }
}


/* 
  ls \   
  -l \q
  */
void executeCommandsFromEditor(char *buf)
{
  
      char **tokens = sst_split_line(buf, "\\");
      int i = 1;
      char *newBuf = malloc(sizeof(char)*100);
      strcpy(newBuf,tokens[0]);
      while(tokens[i] != NULL)
      {
            strcat(newBuf," ");
            strcat(newBuf,tokens[i]);
            i++;
      }
      checkForCommands(newBuf);
}

void cat2Function(char *line)
{
      char **tokens = sst_split_line(line, ">");
      char *outputFileName = tokens[1];
      outputFileName = strtok(outputFileName," ");

      int endFlag = 1;
      int enterFlag = 0;
      int i=0;

      FILE *fptr;

      fptr = fopen(outputFileName, "w+");

      while(endFlag)
      {
            char c = getchar();
            if(enterFlag && c == 'q')
            {
                  endFlag = 0;
                  fclose(fptr);
            }
            else if(enterFlag == 1)
            {
                  enterFlag = 0;
                  fprintf(fptr, "\\");
            }
            if(c == '\\')
            {
                  enterFlag = 1;
            }
            else
            {
                  fprintf(fptr,"%c",c);
            }
      }
}

void checkIFSyntax(char *line)
{
  
      char **tokens=sst_split_line(line," ");
      int i = 0;
      int elseCount=0;
      while(tokens[i+1] != NULL)
      {
            if(strcmp(tokens[i],"else") == 0)
            elseCount++;
            i++;
      }
      if(strcmp(tokens[i],"fi") == 0 && elseCount <=1)
      {
            printf("Valid Input\n");
      }
      else
      {
            printf("Syntax Error\n");
      }
}

void printZeroSizeFiles()
{
      char *buf = malloc(sizeof(char) * 100);
      size_t size = 100;
      if(getcwd(buf, size) == NULL) //gives cwd
      {
            printf("Error\n");
            return;
      }
      struct stat statbuf;
      struct dirent *dirp;
      DIR *dp; 
      dp = opendir(buf); //open the directory that getcwd gave
      if(dp == NULL)
      {
            printf("Error\n");
            return;
      }
      else
      {
            while((dirp = readdir(dp)) != NULL) //Traverse the directory
            {
                  char *path = malloc(sizeof(char)*100);
                  strcpy(path,buf);
                  strcat(path,"/");
                  strcat(path,dirp->d_name); //storing the path of the file
                  stat(path, &statbuf); //gets inode structure for the file
                  if(S_ISREG(statbuf.st_mode)) //checking for regular file
                  {
                        if(statbuf.st_size == 0) //checking for empty files
                        {
                              printf("%s\t%ld\n",dirp->d_name,statbuf.st_size);
                        } 
                  }
            }
      }
}

struct fileInfo
{
      char *name;
      time_t mtime; //storing inode modification time
};

void sortWithINodeTime()
{
      struct fileInfo *displayInfo = malloc(sizeof(struct fileInfo)*100);
      int i = 0;
      char *buf = malloc(sizeof(char) * 100);
      size_t size = 100;
      if(getcwd(buf, size) == NULL)
      {
            printf("Error\n");
            return;
      }
      struct stat statbuf;
      struct dirent *dirp;
      DIR *dp;
      dp = opendir(buf);
      if(dp == NULL)
      {
            printf("Error\n");
            return;
      }
      else
      {
            while((dirp = readdir(dp)) != NULL)
            {
                  char *path = malloc(sizeof(char)*100);
                  strcpy(path,buf);
                  strcat(path,"/");
                  strcat(path,dirp->d_name);
                  stat(path, &statbuf);
                  displayInfo[i].name = malloc(sizeof(char)*strlen(dirp->d_name));
                  strcpy(displayInfo[i].name,dirp->d_name);
                  displayInfo[i].mtime = statbuf.st_ctime; //ctime in stat structure gives inode modification time
                  i++;
            }
            //Sort according to mtime
            int j,k;
            for(j = 0 ; j < i-1 ; j++)
            {
                  for(k = 0 ; k < i - j - 1 ; k++)
                  {
                        if(displayInfo[k].mtime > displayInfo[k+1].mtime)
                        {
                              struct fileInfo temp = displayInfo[k];
                              displayInfo[k] = displayInfo[k+1];
                              displayInfo[k+1] = temp;
                        }
                  }
            }

            //print
            for(j = 0 ; j < i ; j++)
            {
                  char *str = malloc(sizeof(char)*36);
                  strftime(str, 36, "%d.%m.%Y %H:%M:%S", localtime(& displayInfo[j].mtime));
                  printf("%s\t%s\n",displayInfo[j].name,str);
            }
      }
}


void pipeInputOutput(char *line)
{

      int i = 0;
      int flagLessThan = 0;
      int flagGreaterThan = 0;
      while(line != NULL)
      {
            if(line[i] == '<') //Input File
            {
                  flagLessThan = 1;
                  break;
            }
            if(line[i] == '>') //Output File
            {
                  flagGreaterThan = 1;
                  break;
            }
            i++;
      }
      if(flagGreaterThan)
      {
            char **token = sst_split_line(line,"<");
            token[1]=strtok(token[1]," "); //name of the input file file
            char **tokenAgain = sst_split_line(token[0],">"); 
            tokenAgain[1] = strtok(tokenAgain[1]," "); //name of the output file 

            redirectionLessThan = 0;
            redirectionGreaterThan = 0;

            pid_t x;
            int fd0,fd1;
            x = fork();
            if (x == 0) 
            {
                  fd0=open(token[1], O_RDONLY); 
                  dup2(fd0,STDIN_FILENO);
                  close(fd0);

                  fd1 = creat(tokenAgain[1],0644); //create the output File

                  dup2(fd1,STDOUT_FILENO);
                  close(fd1);

                  char **token1=sst_split_line(token[0],"|");
                  parsePipedInput(token1); //execute the piping commands
                  _exit(1);

            } 
            else
            {
                  close(fd0);
                  close(fd1);
                  wait(NULL);
            }
      }
      else
      {
            char **token = sst_split_line(line,">"); 
            token[1]=strtok(token[1]," "); //name of the output File
            char **tokenAgain = sst_split_line(token[0],"<");
            tokenAgain[1] = strtok(tokenAgain[1]," "); //name of the input File

            redirectionLessThan = 0;
            redirectionGreaterThan = 0;

            pid_t x;
            int fd0,fd1;
            x = fork();
            if (x == 0) 
            {
                  fd0=open(tokenAgain[1], O_RDONLY);
                  dup2(fd0,STDIN_FILENO);
                  close(fd0);

                  fd1 = creat(token[1],0644);

                  dup2(fd1,STDOUT_FILENO);
                  close(fd1);

                  char **token1=sst_split_line(token[0],"|");
                  parsePipedInput(token1);
                  _exit(1);

            } 
            else 
            {
                  close(fd0);
                  close(fd1);
                  wait(NULL);
            }
      }
}

void pipeAndOutput(char * line)
{
      char **token = sst_split_line(line,">"); 
      token[1]=strtok(token[1]," "); //gets the output file
      redirectionGreaterThan = 0;
      int fd;
      pid_t x;
      x = fork();
      if (x == 0)
      {
            fd = creat(token[1],0644); //open the output file
            close(STDOUT_FILENO);
            fcntl(fd,F_DUPFD,STDOUT_FILENO);
            char **args = sst_split_line(token[0],"|");
            parsePipedInput(args);
            _exit(1);
      }
      else
      {
            wait(NULL);
      }
}

void pipeAndInput(char * line)
{
      char **token = sst_split_line(line,"<"); 
      token[1]=strtok(token[1]," "); //gets input file name
      redirectionLessThan = 0;
      
      int fd0;
      pid_t x;

      x = fork();
      if (x == 0) 
      {
            fd0=open(token[1], O_RDONLY); //Open file with fd - 0
            dup2(fd0,STDIN_FILENO);
            close(fd0);

            char **token1=sst_split_line(token[0],"|");
            parsePipedInput(token1);
            _exit(1);
      } 
      else 
      {
            wait(NULL);
      }
}

void onlyRedirection(char *line)
{
      int i = 0;
      int flagLessThan = 0;
      int flagGreaterThan = 0;
      while(line != NULL)
      {
            if(line[i] == '<')
            {
                  flagLessThan = 1;
                  break;
            }
            if(line[i] == '>')
            {
                  flagGreaterThan = 1;
                  break;
            }
            i++;
      }

      if(flagGreaterThan)
      {
            char **token = sst_split_line(line,"<");
            token[1]=strtok(token[1]," "); //gets input flag
            char **tokenAgain = sst_split_line(token[0],">");
            tokenAgain[1] = strtok(tokenAgain[1]," "); //gives output file
            redirectionLessThan = 0;
            redirectionGreaterThan = 0;

            pid_t x;
            int fd0,fd1;
            x = fork();
            if (x == 0) 
            {
                  fd0=open(token[1], O_RDONLY); 
                  dup2(fd0,STDIN_FILENO);
                  close(fd0);

                  fd1 = creat(tokenAgain[1],0644); //create the output file

                  dup2(fd1,STDOUT_FILENO);
                  close(fd1);
                
                  char **token1=sst_split_line(token[0]," ");
                  execvp(token1[0], token1);
                  perror("execvp");
                  _exit(1);
            } 
            else
            {
                  wait(NULL);
            }
      }
      else
      {
            char **token = sst_split_line(line,">");
            token[1]=strtok(token[1]," "); //gets output file 
            char **tokenAgain = sst_split_line(token[0],"<");
            tokenAgain[1] = strtok(tokenAgain[1]," "); //gets input file

            redirectionLessThan = 0;
            redirectionGreaterThan = 0;

            pid_t x;
            int fd0,fd1;
            x = fork();
            if (x == 0) 
            {
                  fd0=open(tokenAgain[1], O_RDONLY);
                  dup2(fd0,STDIN_FILENO);
                  close(fd0);

                  fd1 = creat(token[1],0644);

                  dup2(fd1,STDOUT_FILENO);
                  close(fd1);

                  char **token1=sst_split_line(token[0]," ");
                  execvp(token1[0], token1);
                  perror("execvp");
                  _exit(1);
            } 
            else 
            {
                  wait(NULL);
            }
      }
}

void readFromInputFile(char *line)
{
      char **token = sst_split_line(line,"<");
      token[1]=strtok(token[1]," "); //gives input file name
      redirectionLessThan = 0;
      int fd0;
      pid_t x;
      x = fork();
      if (x == 0) 
      {
            fd0=open(token[1], O_RDONLY); //opens the input file
            dup2(fd0,STDIN_FILENO);
            close(fd0);

            char **args = sst_split_line(token[0]," ");
            execvp(args[0], args);
            perror("execvp");
            _exit(1);
      }
      else 
      {
            wait(NULL);
      }
}

void readFromOutputFile(char *line)
{
      char **token = sst_split_line(line,">");
      token[1]=strtok(token[1]," "); //gets output file name
      redirectionGreaterThan = 0;
      int fd0;
      pid_t x;
      x = fork();
      if (x == 0)
      {
            fd0 = creat(token[1],0644);
            dup2(fd0,STDOUT_FILENO);
            close(fd0);
            char **args = sst_split_line(token[0]," ");
            execvp(args[0], args);
            perror("execvp");
            _exit(1);
      }
      else
      {
            wait(NULL);
      }
}


void parsePipedInput(char **token)
{
      int i=0;
      pipeInInputFlag = 0;
      int pipefd[2]; 
      pid_t p1, p2;
      while(token[i+1]!=NULL)
      {
            if (pipe(pipefd) < 0) 
            {
                  printf("\nPipe could not be initialized");
                  return;
            }
            p1 = fork();
            if (p1 < 0) 
            {
                  printf("\nCould not fork");
                  return;
            }
            if (p1 == 0) 
            {
                  close(pipefd[0]); //closing the write end
                  dup2(pipefd[1], STDOUT_FILENO); //opening the read end
                  close(pipefd[1]);
                  char **args = sst_split_line(token[i], " ");
                  if (execvp(args[0], args) < 0) 
                  {
                        printf("\nCould not execute command..\n");
                  }
            } 
            else 
            {
                  p2 = fork();
                  if (p2 < 0) 
                  {
                        printf("\nCould not fork");
                        return;
                  }
                  if (p2 == 0) 
                  {
                        close(pipefd[1]); //closing the read end
                        dup2(pipefd[0], STDIN_FILENO);
                        close(pipefd[0]); //opening the write end
                        char **args = sst_split_line(token[i+1], " ");
                        if (execvp(args[0], args) < 0) 
                        {
                              printf("\nCould not execute command..");
                        }
                  } 
                  else 
                  {
                        close(pipefd[1]);
                        close(pipefd[0]);
                        wait(NULL);
                        wait(NULL);
                  }
            }
            i++;
      }
}

void printFilesWithRegex(char *regex)
{
  
      wordexp_t p;
      char **w;
      int i;
      wordexp(regex, &p, 0);
      w = p.we_wordv;
      for (i = 0; i < p.we_wordc; i++)
      {
            printf("%s\n", w[i]);
      }
      wordfree(&p);
}

void sst_loop(void)
{
        setenv("SHELL","/bin/ownsh",1);
        int status;
       printf("************************\n\n");
        printf("Welcome to SST shell!\n");
        printf("************************\n\n");
         time_t myTime;

        do 
        {
            char *line;
            char **args;
            char *buf = malloc(sizeof(char) * 100);
            size_t size = 100;
            getcwd(buf,size);
            printf("%s~$ ",buf);
            line = sst_read_line();
            time(&myTime); //gets the current time
            char *t = (char*)malloc(sizeof(char)*200);
            strcpy(t,ctime(&myTime)); //store the time in t . ctime makes the time readable 
            flag = 0;
            if(head == NULL)
            {
                  head = newNode(line);
                  head->timestamp = (char*)malloc(sizeof(char) * 200);
                  strcpy(head->timestamp,t);
                  tail = head;
                  totalNodes++;
            }
            else
            {
                  struct node *temp = newNode(line);
                  temp->next = NULL;
                  temp->prev = tail;
                  tail->next = temp;
                  temp->timestamp = (char*)malloc(sizeof(char) * 200);
                  strcpy(temp->timestamp,t);
                  tail = temp;
                  if(totalNodes <= 25)
                  {
                        totalNodes++;
                  }
                  else
                  {
                        struct node *temp = head; //moving head to the second node and freeing the first node. 
                                                //Later the new node will be added after tail
                        head = head->next;
                        head -> prev = NULL;
                        free(temp);
                  }
            }

            status= checkForCommands(line);

            free(line);
            free(args);
            }while (status);
}


int main(int argc, char **argv)
{
      aliasArray = (struct alias*)malloc(sizeof(struct alias) * 10) ; //Maximum 10 aliases

      sst_loop();
      return EXIT_SUCCESS;
}
