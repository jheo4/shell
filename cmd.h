#pragma once

#define MAX_ARGS 10
#define MAX_PATH 260

extern char blank_symbols[];
extern char command_symbols[];

struct cmd{
  int type;
};

struct executed_cmd{
  int type;
  char *argv[MAX_ARGS];
};


struct redirected_cmd{
  int type;
  struct cmd* sub_cmd;
  char* file_name;
  int file_mode;
  int file_desc;
};


struct piped_cmd{
  int type;
  struct cmd* left_end;
  struct cmd* right_end;
};


int get_cmd(char* buf, int buf_size);
void run_cmd(struct cmd* input_cmd);

struct cmd* parse_cmd(char* input_cmd);
struct cmd* parse_line(char** input_buf, char* input_buf_end);
struct cmd* parse_pipe(char** input_buf, char* input_buf_end);
struct cmd* parse_exec(char** input_buf, char* input_buf_end);
struct cmd* parse_redir(struct cmd* input_cmd, char** input_buf,
                        char* input_buf_end);

int get_token(char** input_buf, char* input_buf_end, char** pure_input,
              char** token);
int peek(char** input_buf, char* input_buf_end, char* command_symbols);
void remove_blank_symbols(char** input_buf, char* iput_buf_end);

char* make_copy(char* input_buf, char* input_buf_end);

struct cmd* execute_cmd();
struct cmd* redirect_cmd(struct cmd* sub_cmd, char* file_name, int type);
struct cmd* pipe_cmd(struct cmd* left_end, struct cmd* right_end);

