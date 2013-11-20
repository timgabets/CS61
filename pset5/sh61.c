/**
 * CS61 Problem Set 5. Shell.
 * 
 * In the pset4 there was implemented fork. In this assignment, fork - and several 
 * other interesting system calls - are used to build an important application: 
 * the sh61 shell! This shell will read commands on its standard input and execute
 * them. The simple commands, background commands, conditional commands (&& and ||),
 * redirections and pipes should be implemented, as well as command interruption. 
 * The shell implements a subset of the bash shell’s syntax, and is generally 
v * compatible with bash for the features they share.
 * 
 * Ricardo Contreras HUID 30857194 <ricardocontreras@g.harvard.edu>
 * Tim Gabets HUID 10924413 <gabets@g.harvard.edu>
 * 
 * November 2013
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define TOKEN_CONTROL       0  // token is a control operator,
							   // and terminates the current command
#define TOKEN_NORMAL        1  // token is normal command word
#define TOKEN_REDIRECTION   2  // token is a redirection operator

// parse_shell_token(str, type, token)
//    Parse the next token from the shell command `str`. Stores the type of
//    the token in `*type`; this is one of the TOKEN_ constants. Stores the
//    token itself in `*token`. The token is a newly-allocated string that
//    should be freed eventually with `free`. Returns the pointer within
//    `str` to the next token.
//
//    Returns NULL and sets `*token = NULL` at the end of string.

char* parse_shell_token(char* str, int* type, char** token);
int pipeused = 0;		// 1 if previous command used pipe for writing. 
						// Means that current command need to use pipe for reading.

/**
 * Data structure describing a command. Add your own stuff.
 */
typedef struct command command;
struct command {
	int     argc;           // number of arguments
	char**  argv;           // arguments, terminated by NULL
	// in a better world all these should be binary flags:
	int     run;            // should the command be run or not. Useful in case of logical operations - &&, ||, etc.
	int     background;     // 1 if command should be run in background, 0 otherwise
	int     piperead;       // 1 if command should read from pipe
	int     pipewrite;      // 1 if command should write to pipe
        int     pipefd[2];      // file descriptors for pipe
};

char *lc;    // pointer to last command

/**
 * [command_alloc allocates and returns a new command structure.]
 * @return  [new command structure]
 */
static command* command_alloc(void) {
	command* c = (command*) malloc(sizeof(command));
	c->argc = 0;
	c->argv = NULL;
	c -> run = 0;
	c -> background = 0;
	c -> pipewrite = 0;
	c -> piperead = 0;
	return c;
}

/**
 * [command_free Free command structure `c`, including all its words.]
 * @param c [command structure]
 */
static void command_free(command* c) {
	for (int i = 0; i != c->argc; ++i)
		free(c->argv[i]);
	free(c->argv);
	free(c);
}


/**
 * [command_append_arg Add `word` as an argument to command `c`. This increments `c->argc` 
 *                     and arguments `c->argv`]
 * @param c    [command]
 * @param word [description]
 */
static void command_append_arg(command* c, char* word) {
	c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
	c->argv[c->argc] = word;
	c->argv[c->argc + 1] = NULL;
	++c->argc;
}


/**
 * COMMAND PARSING
 */
typedef struct buildstring {
	char* s;
	int length;
	int capacity;
} buildstring;


/**
 * [buildstring_append    Add `ch` to the end of the dynamically-allocated string `bstr->s`.]
 * @param bstr [dynamically-allocated string ]
 * @param ch   [description]
 */
void buildstring_append(buildstring* bstr, int ch) {
	if (bstr->length == bstr->capacity) {
		int new_capacity = bstr->capacity ? bstr->capacity * 2 : 32;
		bstr->s = (char*) realloc(bstr->s, new_capacity);
		bstr->capacity = new_capacity;
	}
	bstr->s[bstr->length] = ch;
	++bstr->length;
}


/**
 * [isshellspecial  Test if `ch` is a command that's special to the shell (that ends 
 *                  a command word)]
 * @param  ch [description]
 * @return    [description]
 */
static inline int isshellspecial(int ch) {
  return ch == '<' || ch == '>' || ch == '&' || ch == '|' || ch == ';'
		|| ch == '(' || ch == ')' || ch == '#';
}


/**
 * [parse_shell_token description]
 * @param  str   [description]
 * @param  type  [description]
 * @param  token [description]
 * @return       [description]
 */
char* parse_shell_token(char* str, int* type, char** token) {
	buildstring buildtoken = { NULL, 0, 0 };

	// skip spaces; return NULL and token ";" at end of line
	while (str && isspace((unsigned char) *str))
		++str;
	if (!str || !*str || *str == '#') {
		*type = TOKEN_CONTROL;
		*token = NULL;
		return NULL;
	}

	// check for a redirection or special token
	for (; isdigit((unsigned char) *str); ++str)
		buildstring_append(&buildtoken, *str);
	if (*str == '<' || *str == '>') {
		*type = TOKEN_REDIRECTION;
		buildstring_append(&buildtoken, *str);
		if (*str == '>' && str[1] == '>') {
			buildstring_append(&buildtoken, *str);
			str += 2;
		} else
			++str;
	} else if (buildtoken.length == 0
			   && (*str == '&' || *str == '|')
			   && str[1] == *str) {
		*type = TOKEN_CONTROL;
	//printf("//& or or//");
		buildstring_append(&buildtoken, *str);
		buildstring_append(&buildtoken, str[1]);
		str += 2;
	} else if (buildtoken.length == 0
			   && isshellspecial((unsigned char) *str)) {
		*type = TOKEN_CONTROL;
		buildstring_append(&buildtoken, *str);
		++str;
	} else {
		// it's a normal token
		*type = TOKEN_NORMAL;
		int quoted = 0;
		// Read characters up to the end of the token.
		while ((*str && quoted)
			   || (*str && !isspace((unsigned char) *str)
				   && !isshellspecial((unsigned char) *str))) {
			if (*str == '\"')
				quoted = !quoted;
			else if (*str == '\\' && str[1] != '\0') {
				buildstring_append(&buildtoken, str[1]);
				++str;
			} else
				buildstring_append(&buildtoken, *str);
			++str;
		}
	}

	// store new token and return the location of the next token
	buildstring_append(&buildtoken, '\0'); // terminating NULL character
	*token = buildtoken.s;
	return str;
}



/**
 * [eval_command description]
 * @param c [description]
 */
void eval_command(command* c) {
	pid_t pid = -1;             // process ID for child

	// checking for '&''
	for(int i = 0; i < c -> argc; i++)
		if(c -> argv[i][0] == '&')
		{
			c -> background = 1;
			c -> argv[i] = NULL;
			c -> argc = i;
			break;
		}
	
	// checking for ';''
	for(int i = 0; i < c -> argc; i++)
		if(c -> argv[i][0] == ';')
		{
			c -> background = 0;
			c -> argv[i] = NULL;
			c -> argc = i;
			break;
		}

	
	//if(pipeused)
	if(c -> pipewrite == 1)
	  pipe(c->pipefd);

	pid = fork();
	if(pid == 0)
	{
		// child
	  if(c -> pipewrite == 1 && c -> piperead == 0)
	    //if(c -> pipewrite == 1)
	  {
	    //printf("write c: %p lc: %p\n", c, lc);
	    //c -> background = 1;
	    close(c->pipefd[0]); // close unused read end for first child
			// making stdout the pipe's write end
			dup2(c->pipefd[1], STDOUT_FILENO);
			close(c->pipefd[1]); // no need keep 2 copies of pipe
		}

	  if(c -> piperead == 1 && c -> pipewrite == 0)
		{
		  //printf("read  c: %p lc: %p\n", c, lc);
		  //c -> background = 1;
		  command *lastCommand = (command*)lc; 
		    close(lastCommand->pipefd[1]); // close unused write end of second child
		    // make second child's stdin the pipe's read end
    		dup2(lastCommand->pipefd[0], STDIN_FILENO);
    		close(lastCommand->pipefd[0]); // no need keep 2 copies of pipe
		}

        // TODO: complicated case - the command is both reading and writing
        if(c -> piperead == 1 && c -> pipewrite == 1)
	{
	  //printf("both  c: %p lc: %p\n", c, lc);
	  //c -> background = 1;
	  close(c->pipefd[0]); // close unused read end for first child
	  // making stdout the pipe's write end
	  dup2(c->pipefd[1], STDOUT_FILENO);
	  close(c->pipefd[1]); // no need keep 2 copies of pipe
	
	  command *lastCommand = (command*)lc; 
	  close(lastCommand->pipefd[1]); // close unused write end of second child
	  // make second child's stdin the pipe's read end
	  dup2(lastCommand->pipefd[0], STDIN_FILENO);
	  close(lastCommand->pipefd[0]); // no need keep 2 copies of pipe
	  
	  }
 
		// detecting special characters
		for(int i = 0; i < c -> argc; i++)
		{
	  
			switch( c -> argv[i][0])
			{
			case '<': 
			        // reassigning standard file descriptors:
				close(STDIN_FILENO);
				open(c -> argv[i + 1], O_RDONLY);
				c -> argv[i] = NULL;
				break;
			
			case '>':   
				// reassigning standard file descriptors:
				close(STDOUT_FILENO);
				open(c -> argv[i + 1], O_RDONLY);
				c -> argv[i] = NULL;
				break;
			};
		}

		if( execvp(c -> argv[0], c -> argv) == -1)
		{    
			perror( strerror(errno) );
			exit(-1);
		}

	}else
	{
        // parent
	  /*
	if(c -> piperead == 1 && c -> pipewrite == 1)
	//if(pipeused == 0)
        {
	  command *lastCommand = (command*)llc; 
	  //if(lastCommand -> pipewrite == 1)
	//if(pipeused == 0)
	  //{  
            // closing pipe
	    close(lastCommand->pipefd[0]);
	    close(lastCommand->pipefd[1]);
	    //command_free(lastCommand);
	    //llc = NULL;
	    } else*/ if(c -> piperead == 1)
	{
	    command *lastCommand = (command*)lc;
	    if(lastCommand -> pipewrite == 1)
	      {     
		lc = (char*)c;
		close(lastCommand->pipefd[0]);
		close(lastCommand->pipefd[1]);
		command_free(lastCommand);
		//lc = NULL;
	      }
	 }
 	          
        if(c -> background == 0)
            waitpid(pid, NULL, 0);
    }
	//printf("XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\n");
}

/**
 * [build_execute description]
 * @param commandLine [command to build and execute]
 */
void build_execute(char* commandList) {
    int type;
    char* token;

    // build the command
    command* c = command_alloc();
    while ((commandList = parse_shell_token(commandList, &type, &token)) != NULL)
    {
        //printf("token: %s\n", token);
        command_append_arg(c, token);
		
        if(pipeused)
        {
            // previos command used pipe for writing
            c -> piperead = 1;
            pipeused = 0;
        }

        if(*token == '|')
        {
            c -> pipewrite = 1;
            c -> argc--;
            c -> argv[ c -> argc ] = NULL;
            pipeused = 1;
	    //llc = lc;
	    if (c->piperead != 1)
	      lc = (char*)c;

	    if (c -> argc)
	      eval_command(c);
	    return;
        }
    }
    // execute the command
    if (c -> argc)
        eval_command(c);
    if (c->pipewrite != 1)
      command_free(c);
}


/**
 * [eval_command_line description]
 * @param s [description]
 */
void eval_command_line(const char* s) {

	// Iterate through s string
	int start = 0;
	int length = strlen(s);
	int insideParenthesis = 0;
	for (int i = 0; i < length; i++)
	{
	  //printf("char %i: %c\n", i,s[i] );
	        // Check if we are inside of parenthesis
		if (s[i] == '"') 
		{
			insideParenthesis ++;
			if (insideParenthesis == 2) 
			   insideParenthesis = 0;
		}

		// If we are not inside of a parenthesis and 
		//it is separated by ; or & ...
		if ((insideParenthesis != 1) && (s[i] == ';' || s[i] == '&' || s[i] == '|')) 
		{
			// Create command list from the start of last command list

			char *commandList = (char*) malloc(i - start + 2);
			memset(commandList, '\0', sizeof(i - start + 2));
			strncpy(commandList, s + start, i - start + 1);
			
			//printf("commandList: %s from: %i to %i\n", commandList, start, i - start);
			// build and execute command list
			build_execute(commandList);
			free(commandList);
			start = i + 1;
		}
	}

	// Create and execute the last comand list also
	char* commandList = (char*) malloc(length - start + 2);
	strncpy(commandList, s + start, length - start + 1);
	//printf("commandList: %s\n", commandList);
	build_execute(commandList);
	free(commandList);
}
	

/**
 * [set_foreground     Tell the operating system that `p` is the current foreground process
 *                     for this terminal. This engages some ugly Unix warts, so we provide
 *                     it for you]
 * @param  p [description]
 * @return   [description]
 */
int set_foreground(pid_t p) {
	// YOU DO NOT NEED TO UNDERSTAND THIS.
	static int ttyfd = -1;
	if (ttyfd < 0) {
		// We need a fd for the current terminal, so open /dev/tty.
		int fd = open("/dev/tty", O_RDWR);
		assert(fd >= 0);
		// Re-open to a large file descriptor (>=10) so that pipes and such
		// use the expected small file descriptors.
		ttyfd = fcntl(fd, F_DUPFD, 10);
		assert(ttyfd >= 0);
		close(fd);
		// The /dev/tty file descriptor should be closed in child processes.
		fcntl(ttyfd, F_SETFD, FD_CLOEXEC);
	}
	// `p` is in its own process group.
	int r = setpgid(p, p);
	if (r < 0)
		return r;
	// The terminal's controlling process group is `p` (so processes in group
	// `p` can output to the screen, read from the keyboard, etc.).
	return tcsetpgrp(ttyfd, p);
}


int main(int argc, char* argv[]) {
	FILE* command_file = stdin;
	int quiet = 0;
	int r = 0;

	// Check for '-q' option: be quiet (print no prompts)
	if (argc > 1 && strcmp(argv[1], "-q") == 0) {
		quiet = 1;
		--argc, ++argv;
	}

	// Check for filename option: read commands from file
	if (argc > 1) {
		command_file = fopen(argv[1], "rb");
		if (!command_file) {
			perror(argv[1]);
			exit(1);
		}
	}

	char buf[BUFSIZ];
	int bufpos = 0;
	int needprompt = 1;

	while (!feof(command_file)) {
		// Print the prompt at the beginning of the line
		if (needprompt && !quiet) {
			printf("sh61[%d]$ ", getpid());
			fflush(stdout);
			needprompt = 0;
		}

		// Read a string, checking for error or EOF
		if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
			if (ferror(command_file) && errno == EINTR) {
				// ignore EINTR errors
				clearerr(command_file);
				buf[bufpos] = 0;
			} else {
				if (ferror(command_file))
					perror("sh61");
				break;
			}
		}

		// If a complete command line has been provided, run it
		bufpos = strlen(buf);
		if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
			eval_command_line(buf);
			bufpos = 0;
			needprompt = 1;
		}

		// Handle zombie processes and/or interrupt requests
		// Your code here!
	}

	return 0;
}
