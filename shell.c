#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "cmd.h"


int main(int argc, char *argv[])
{
  sigset_t blockset;
  sigemptyset(&blockset);
  sigaddset(&blockset, SIGINT); //block : ^Csignal
  sigaddset(&blockset, SIGQUIT);//block : ^\signal
  sigprocmask(SIG_BLOCK, &blockset, NULL);

  static char buf[100];
  int pid;

  while(1){
    if(get_cmd(buf, sizeof(buf)) < 0)
      break;

    // cd must be embedded in shell
    if(buf[0]=='c' && buf[1]=='d' && buf[2]==' '){
      buf[strlen(buf)-1] = 0;

      int chdir_ret = chdir(buf+3);
      if(chdir_ret < 0){
        fprintf(stderr, "cannot cd %s\n", buf+3);
      }
      continue;
    }

    if((buf[0]=='e' || buf[0]=='q') && (buf[1]=='x' || buf[1]=='u')
        && buf[2] == 'i' && buf[3] == 't')
      return 0;


    pid = fork();
    if(pid < 0){
      perror("fork");
    }
    else if(pid == 0){
      run_cmd(parse_cmd(buf));
    }
    wait(NULL);
  }
  return 0;
}

