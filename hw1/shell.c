#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens *tokens);
int cmd_help(struct tokens *tokens);
int cmd_wait(struct tokens *tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens *tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_exit, "exit", "exit the command shell"},
  {cmd_wait, "wait", "wait all child process exit !"}
};

int ignore_signals[] = {
	SIGINT,SIGQUIT,SIGTSTP,SIGTERM,SIGTTIN,SIGCONT,SIGTTOU
};
char *PATH;
/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens *tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens *tokens) {
  exit(0);
}

int cmd_wait(unused struct tokens *tokens) {
	int status = 0;
	while(1) {
		if (wait(&status) == -1) {
			break;
		}
	}
	return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* shell的初始化函数*/
void init_shell() {
 // STDIN_FILENO 标准输入文件句柄 类型int
 // 与stdin不同 stdin 类型是File *
  shell_terminal = STDIN_FILENO;

  // isatty 根据文件句柄判断是否为设备文件 若是则返回1 否则返回0
  // 另外标准输入输出文件均为设备文件
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
  	// tcgetprg 获取当前前台进程的进程组id
  	// getpgrp 获取当前进程的进程组id
  	// 若shell进程不是前台进程 则向shell进程发送终止信号 暂停shell
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    shell_pgid = getpid();

    tcsetpgrp(shell_terminal, shell_pgid);

    // 保存终端的运行信息
    tcgetattr(shell_terminal, &shell_tmodes);
    // shell进程屏蔽中断信号
    // 只能通过exit退出
    for(int i = 0; i < sizeof(ignore_signals) ; i++) {
    	signal(ignore_signals[i],SIG_IGN);
    }
  }
}

int run_program_by_envpath(char *command, char *argvlist[]) {
	PATH = getenv("PATH");
	if(PATH == NULL) {
		return -1;
	}
	char prog_path[4096];
	char *path_dir = strtok(PATH, ":");
	while (path_dir != NULL) {
		sprintf(prog_path, "%s/%s", path_dir, command);
		if (access(prog_path, F_OK) != -1) {
		  return execv(prog_path, argvlist);
		}
		path_dir = strtok(NULL, ":");
	}
	return -1;

}
// 1 父进程fork，等待子进程返回 
// 2 子进程exec
int fork_and_exec(struct tokens * tokens) {
	      /* REPLACE this to run commands as programs. */
	int status = -1;
	int token_length = tokens_get_length(tokens);
	// 空命令不用fork
	if(token_length == 0) {
		return 0;
	}
	pid_t pid = fork();
	int run_back = token_length > 1 && strcmp(tokens_get_token(tokens,token_length-1),"&") == 0;
	if (pid > 0) {
		// 父进程
		// 父进程等待子进程结束 销毁并回收子进程的资源
		// fork的子进程如果是后台进程则直接父进程waitpid直接返回
		// WNOHUNG 若没有子进程结束则立刻返回
		// WUNTRACED 立即返回，之后会记录子进程的返回值到status
		int flag = run_back ? WNOHANG:0;
		waitpid(pid,&status,flag | WUNTRACED);
		tcsetpgrp(shell_terminal,getpgrp());
	} else if (pid == 0) {
		// 子进程
		pid_t sub_pid = getpid();
		// int segpgid(pid_t pid,pid_t pgid)
		// 当pid = 0， 会使用调用该函数的进程的pid
		// 当pgid = 0, 会将该进程pid的值设为pgid
	
		int i;
		int idx = 0;

    	char * command = tokens_get_token(tokens,0);
    	char ** argvlist = malloc((token_length + 1) * sizeof(char *));
    	for (i = 0; i < token_length;i++) { 
    		argvlist[i] = tokens_get_token(tokens,i);
    		// 查找重定向符号
    		if (strcmp(argvlist[i],"<") == 0 || strcmp(argvlist[i],">") == 0) {
    			idx = i;
    			break;
    		}
    	}
    	// 新建进程组，进程组号为当前进程号
    	setpgid(0,sub_pid); // 或者 setpgid(0,0);

    	// 判断是否后台运行
    	if(run_back) {
			tcsetpgrp(shell_terminal,getpgrp());
			token_length --;
    	}
    	// 子进程需要响应中断信号
		for(int i = 0; i < sizeof(ignore_signals) ; i++) {
			signal(ignore_signals[i],SIG_DFL);
		}
    	// 忽略 < > 在第一个参数或是最后一个参数
    	// 重定向标准输入或输出
    	if (idx && idx < (token_length-1)) {
    		if(strcmp(argvlist[idx],"<") == 0) {
    			freopen(tokens_get_token(tokens,idx+1),"r",stdin);
    		} else {
    			freopen(tokens_get_token(tokens,idx+1),"w",stdout);
    		}
    		token_length = idx;
    	}
    	argvlist[token_length] = NULL;
    	if (execv(command,argvlist) == -1 && run_program_by_envpath(command,argvlist) == -1 ) {
    		printf("command %s don't exist !\n", command);
    		exit(-1);
    	}
	} else {
		// fork时发生错误
		perror("Fork failed");
		exit(1);
	}
	return 1;
}

int main(unused int argc, unused char *argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;
  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);
  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens *tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
    	fork_and_exec(tokens);
    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }
  return 0;
}

