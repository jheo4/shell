#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>
#include "cmd.h"

char blank_symbols[] = " \t\r\n\v";
char command_symbols[] = "<|>";


int get_cmd(char* buf, int buf_size){
  // isatty: test whether a file desc refers to terminal
  // fileno: map a stream pointer to a file desc
  char cur_dir[MAX_PATH];
  if(isatty(fileno(stdin))){
    getcwd(cur_dir, MAX_PATH);

    fprintf(stdout, "%s$ ", cur_dir);
  }

  memset(buf, 0, buf_size);
  fgets(buf, buf_size, stdin);

  if(buf[0] == 0){
    return -1;
  }
  return 0;
};


void run_cmd(struct cmd* input_cmd){
  int pipe_desc[2];
  struct executed_cmd *ecmd;
  struct piped_cmd *pcmd;
  struct redirected_cmd *rcmd;

  if(input_cmd == 0)
    exit(0);

  switch(input_cmd->type){
  default:
    fprintf(stderr, "unknown command\n");
    exit(-1);

  case ' ':
    ecmd = (struct executed_cmd*)input_cmd;
    if(ecmd->argv[0] == 0){
      exit(0);
    }
    execvp(ecmd->argv[0], ecmd->argv);
    break;

  case '>':
  case '<':
    rcmd = (struct redirected_cmd*)input_cmd;
    int fd = open(rcmd->file_name, rcmd->file_mode,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
    dup2(fd, rcmd->file_desc);
    close(fd);
    run_cmd(rcmd->sub_cmd);
    break;

  case '|':
    pcmd = (struct piped_cmd*)input_cmd;
    //ls | grep : left output to right input.
    if(pcmd->right_end){
      // a | b
      pipe(pipe_desc);
      int pid = fork();
      if(pid == 0){
        dup2(pipe_desc[1], STDOUT_FILENO);
        close(pipe_desc[0]);
        ecmd = (struct executed_cmd*)pcmd->left_end;
        execvp(ecmd->argv[0], ecmd->argv);
      }
      else{
        waitpid(pid, NULL, 0);
        dup2(pipe_desc[0], STDIN_FILENO);
        close(pipe_desc[1]);
        run_cmd(pcmd->right_end);
      }
    }
    else{
      run_cmd(pcmd->left_end);
    }
    break;
  }
  exit(0);
};


struct cmd* parse_cmd(char* input_cmd){
  char* input_cmd_end = input_cmd + strlen(input_cmd);
  struct cmd* parsed_cmd;

  parsed_cmd = parse_line(&input_cmd, input_cmd_end);
  if(input_cmd != input_cmd_end){
    fprintf(stderr, "leftovers: %s\n", input_cmd);
    exit(-1);
  }

  return parsed_cmd;
};


struct cmd* parse_line(char** input_buf, char* input_buf_end){
  struct cmd* ret;
  ret = parse_pipe(input_buf, input_buf_end);

  return ret;
};


struct cmd* parse_pipe(char** input_buf, char* input_buf_end){
  struct cmd* ret;
  ret = parse_exec(input_buf, input_buf_end);
  if(peek(input_buf, input_buf_end, "|")){
    get_token(input_buf, input_buf_end, 0, 0);
    ret = pipe_cmd(ret, parse_pipe(input_buf, input_buf_end));
  }

  return ret;
};


struct cmd* parse_exec(char** input_buf, char* input_buf_end){
  char *pure_input, *token;
  int cmd_symbol, argc;
  struct executed_cmd* execcmd;
  struct cmd* ret;

  ret = execute_cmd();
  execcmd = (struct executed_cmd*)ret;

  argc = 0;

  while(!peek(input_buf, input_buf_end, "|")){
    cmd_symbol = get_token(input_buf, input_buf_end, &pure_input, &token);
    if(cmd_symbol == 0){
      break;
    }
    if(cmd_symbol != 'a'){
      fprintf(stderr, "syntax error\n");
      exit(-1);
    }
    execcmd->argv[argc++] = make_copy(pure_input, token);
    if(argc >= MAX_ARGS){
      fprintf(stderr, "too many args\n");
      exit(-1);
    }

    ret = parse_redir(ret, input_buf, input_buf_end);
  }
  execcmd->argv[argc] = 0;
  return ret;
};


struct cmd* parse_redir(struct cmd* input_cmd, char** input_buf,
                        char* input_buf_end){
  int cmd_symbol;
  char* pure_input, *token;

  while(peek(input_buf, input_buf_end, "<>")){
    cmd_symbol = get_token(input_buf, input_buf_end, 0, 0);
    if(get_token(input_buf, input_buf_end, &pure_input, &token) != 'a'){
      fprintf(stderr, "missing file for redirection\n");
      exit(-1);
    }
    switch(cmd_symbol){
    case '<': // FILE_NAEM < COMMAND
      input_cmd = redirect_cmd(input_cmd, make_copy(pure_input, token), '<');
      break;

    case '>': // COMMAND > FILE_NAME
      input_cmd = redirect_cmd(input_cmd, make_copy(pure_input, token), '>');
      break;
    }
  }
  return input_cmd;
};


int get_token(char** input_buf, char* input_buf_end, char** pure_input,
              char** token){
  remove_blank_symbols(input_buf, input_buf_end);

  char* input = *input_buf;
  int ret;
  if(pure_input){
    *pure_input = input;
  }
  ret = *input;
  switch(*input){
  case 0:
    break;

  case '|':
  case '<':
  case '>':
    input++;
    break;

  default:
    ret = 'a';
    while(input < input_buf_end && !strchr(blank_symbols, *input)
          && !strchr(command_symbols, *input)){
      input++;
    }
    break;
  }
  if(token){
    *token = input;
  }

  remove_blank_symbols(&input, input_buf_end);
  *input_buf = input;
  return ret;
};


int peek(char** input_buf, char* input_buf_end, char* command_symbols){
  //remove_blank_symbols(input_buf, input_buf_end);
  char* input = *input_buf;
  while(input < input_buf_end && strchr(blank_symbols, *input))
    input++;
  *input_buf = input;
  return *input && strchr(command_symbols, *input);
};


void remove_blank_symbols(char** input_buf, char* input_buf_end){
  char *adjusted_input = *input_buf;

  while(adjusted_input < input_buf_end
        && strchr(blank_symbols, *adjusted_input)){
    adjusted_input++;
  }
  *input_buf = adjusted_input;
}


char* make_copy(char* input_buf, char* input_buf_end){
  int buf_size = input_buf_end - input_buf;
  char *copied_buf = malloc(buf_size+1);
  assert(copied_buf);
  strncpy(copied_buf, input_buf, buf_size);
  copied_buf[buf_size] = 0;

  return copied_buf;
};


struct cmd* execute_cmd(){
  struct executed_cmd* ecmd;
  ecmd = malloc(sizeof(*ecmd));
  memset(ecmd, 0, sizeof(*ecmd));
  ecmd->type = ' ';

  return (struct cmd*)ecmd;
};


struct cmd* redirect_cmd(struct cmd* sub_cmd, char* file_name, int type){
  struct redirected_cmd* rcmd;
  rcmd = malloc(sizeof(*rcmd));
  memset(rcmd, 0, sizeof(*rcmd));
  rcmd->type = type;
  rcmd->sub_cmd = sub_cmd;
  rcmd->file_name = file_name;

  // CMD < FILE : O_RDONLY
  // CMD > FILE : O_WRONLY|O_CREAT|O_TRUNC
  rcmd->file_mode = (type == '<') ? O_RDONLY : O_WRONLY|O_CREAT|O_TRUNC;
  rcmd->file_desc = (type == '<') ? 0 : 1;

  return (struct cmd*)rcmd;
};


struct cmd* pipe_cmd(struct cmd* left_end, struct cmd* right_end){
  struct piped_cmd* pcmd;
  pcmd = malloc(sizeof(*pcmd));
  memset(pcmd, 0, sizeof(*pcmd));
  pcmd->type = '|';
  pcmd->left_end = left_end;
  pcmd->right_end = right_end;

  return (struct cmd*)pcmd;
};
