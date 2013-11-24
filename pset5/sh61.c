/**
 * CS61 Problem Set 5. Shell.
 * 
 * In the pset4 there was implemented fork. In this assignment, fork - and several 
 * other interesting system calls - are used to build an important application: 
 * the sh61 shell! This shell will read commands on its standard input and execute
 * them. The simple commands, background commands, conditional commands (&& and ||),
 * redirections and pipes should be implemented, as well as command interruption. 
 * The shell implements a subset of the bash shell’s syntax, and is generally 
 * compatible with bash for the features they share.
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
#include <linux/limits.h>

#define TOKEN_CONTROL       0  // token is a control operator,
							   // and terminates the current command
#define TOKEN_NORMAL        1  // token is normal command word
#define TOKEN_REDIRECTION   2  // token is a redirection operator
#define TOKEN_LOGICAL       3  // token is a logical operator

#define LOGICAL_OR          1  // previous command used || operator
#define LOGICAL_AND         2  // previous command used && operator


// parse_shell_token(str, type, token)
//    Parse the next token from the shell command `str`. Stores the type of
//    the token in `*type`; this is one of the TOKEN_ constants. Stores the
//    token itself in `*token`. The token is a newly-allocated string that
//    should be freed eventually with `free`. Returns the pointer within
//    `str` to the next token.
//
//    Returns NULL and sets `*token = NULL` at the end of string.

char* parse_shell_token(char* str, int* type, char** token);

int set_foreground(pid_t p);

int bufpos = 0;
int needprompt = 1;

int pipeused = 0;	    // 1 if previous command used pipe for writing. 
                            // Means that current command need to use pipe for reading.
int command_result;         // Result of the prevous command. Useful in case of logical operations
int check_previous;         // 0 if previous command was not logical,
                            // LOGICAL_OR (1) if previous command used || operator, and
                            // LOGICAL_AND (2) if previous command used && operator.
int isParent = 1;

int interrupted = 0;    // 1 if user did ctrl+c 0 if runs normal (this kills the foreground group process in the main loop)
int endCommands;    // 1 if user did ctrl+c 0 if runs normal (it avoids eval_command_lin to pass the rest of the commands in case of crt+c)

/**
 * Data structure describing a command. Add your own stuff.
 */
typedef struct command command;
struct command {
	int     argc;           // number of arguments
	char**  argv;           // arguments, terminated by NULL
	// in a better world all these should be binary flags:
	int     run;               // should the command be run or not. Useful in case of logical operations - &&, ||, etc.
	int     background;        // command &
	int     piperead;          // ... | command
    int     pipewrite;         // command | ...
    int     input_redirected;  // commmand < ...
    int     output_redirected; // command > ...
    int     stderr_redirected; // command 2> ...
    int     isParent;
    int     pipefd[2];         // file descriptors for pipe
};

char *lc;    // pointer to last command

pid_t currentPid = -1;

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
    c -> input_redirected = 0;
    c -> output_redirected = 0;
    c -> stderr_redirected = 0;
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
			   && str[1] != *str) {
		*type = TOKEN_CONTROL;
		buildstring_append(&buildtoken, *str);
		buildstring_append(&buildtoken, str[1]);
		str += 2;
	} else if (buildtoken.length == 0
               && (*str == '&' || *str == '|')
               && str[1] == *str) {
        *type = TOKEN_LOGICAL;
        buildstring_append(&buildtoken, *str);
        buildstring_append(&buildtoken, str[1]);
        str += 2;
    } else if (buildtoken.length == 0
			   && isshellspecial((unsigned char) *str)) {
		*type = TOKEN_CONTROL;
		buildstring_append(&buildtoken, *str);
		++str;

    } else if (buildtoken.length == 0
               && *str == '2' && str[1] == '>'){
        *type = TOKEN_REDIRECTION;
        buildstring_append(&buildtoken, *str);
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

    // Saving file descriptors
    int stdin_copy = dup(STDIN_FILENO);
    int stdout_copy = dup(STDOUT_FILENO);
    int stderr_copy = dup(STDERR_FILENO);

    if(c -> pipewrite == 1)
        pipe(c->pipefd);

    // command > ...
    if(c -> output_redirected)          
    {
        // reassigning standard file descriptors:
        close(STDOUT_FILENO);
        if( open(c -> argv[c -> output_redirected], O_CREAT | O_WRONLY | O_TRUNC, 0664 ) == -1)
        {
            perror( strerror(errno) );
            exit(-1);
        }

        c -> argv[c -> argc - 1] = NULL;
        c -> argc--;
    }

    // command < ...
    if(c -> input_redirected)
    {
        // reassigning standard file descriptors:
        close(STDIN_FILENO);
        if(open(c -> argv[c -> input_redirected], O_CREAT | O_RDONLY ) == -1)
        {
            perror( strerror(errno) );
            exit(-1);
        }
        c -> argv[c -> argc - 1] = NULL;
        c -> argc--;
    }

    // command 2> ...
    if(c -> stderr_redirected)          
    {
        // reassigning standard file descriptors:
        close(STDERR_FILENO);
        if(open(c -> argv[c -> stderr_redirected], O_CREAT | O_WRONLY | O_TRUNC, 0664 ) == -1)
        {
            perror( strerror(errno) );
            exit(-1);
        }
        c -> argv[c -> argc - 1] = NULL;
        c -> argc--;
    }

    // Only wait for the last part of a pipe...
    if(c -> piperead == 1 && c -> pipewrite == 0)
    {
      // Set the last part of the pipe to foregreound
      c->background = 0;
    } else if (c -> piperead == 1 && c -> pipewrite == 1)
    {
      // set a middle part of the pipe to background
      c->background = 1;
    } else if (c -> piperead == 0 && c -> pipewrite == 1)
    {
      // set the first part of the pipe to background
      c->background = 1;
    }


    if( strcmp(c -> argv[0], "cd") != 0)
    {
        // Start of command group...
        // change group id
        if (c->isParent == 1) 
        {
            setpgid(0, 0);
        }

    	int parentPid = getpid();

        pid = fork();
        if(pid == 0)
        {       
	    // Write to pipe...
            // use command fd's with apropiate connections
            if(c -> pipewrite == 1)
            {
                close(c->pipefd[0]); 
                dup2(c->pipefd[1], STDOUT_FILENO);
                close(c->pipefd[1]); 
            }

            // Read from pipe...
            // use last command fd's with apropiate connections
            if(c -> piperead == 1)
            {
                command *lastCommand = (command*)lc; 
                close(lastCommand->pipefd[1]); 
                dup2(lastCommand->pipefd[0], STDIN_FILENO);
                close(lastCommand->pipefd[0]);
        
                // ... | command | ...
                // Update the last command pointer.
                if(c -> piperead == 1 && c -> pipewrite == 1)
                {
                    command *lastCommand = (command*)lc; 
                }
            }
    
	    // Execute command
            if( execvp(c -> argv[0], c -> argv) == -1){    
                perror( strerror(errno) );
                exit(-1);
            }

        }else
        {
            // PARENT
            // If command reads from a pipe...
            if(c -> piperead == 1)
            {
                // and last command writes to a pipe...
                command *lastCommand = (command*)lc;
                if(lastCommand -> pipewrite == 1)
                {     
                    // Make current command last command pointer
                    lc = (char*)c;
                    // Close last commands fds
                    close(lastCommand->pipefd[0]);
                    close(lastCommand->pipefd[1]);
                    // free last command
                    command_free(lastCommand);
                }
            }
	    
	    // If it parent command (start of a command group)...
	    //but it is not a pipe... set_foreground to parent pid
	    if (c->isParent == 1  && c->pipewrite != 0 && c -> background == 0) 
	    {
	        currentPid = parentPid;
		signal(SIGTTOU, SIG_IGN);
		set_foreground(parentPid);
	    }
	    // If it parent command (start of a command group)...
	    // and is the start of a pipe... set_foreground to parent pid
	    if (c->isParent == 1  && c->pipewrite == 0 && c -> background == 1) 
	    {
	        currentPid = parentPid;
		signal(SIGTTOU, SIG_IGN);
		set_foreground(parentPid);
	    }
     	          
            if(c -> background == 0)
	    {
	        // Wait for foreground process 
	        waitpid(pid, &command_result, 0);
	    } else 
	    {
	        // Dont hang for background process
	        waitpid(pid, &command_result, WNOHANG);
	    }
	
	    // If it parent command (start of a command group)...
	    //but it is not a pipe... set_foreground to current pid
	    if (c->isParent == 1 && c->pipewrite != 0 && c -> background == 0) 
	    {
	        signal(SIGTTOU, SIG_IGN);
		set_foreground(getpid());
	    }
	    
	    // If it parent command (start of a command group)...
	    // and is the start of a pipe... set_foreground to current pid
	    if(c -> piperead == 1 && c -> pipewrite == 0)
	    {
	        signal(SIGTTOU, SIG_IGN);
		set_foreground(getpid());
	    }
	    
	}
    } 
    // cd, actually (first it checks that we are not going into a pipe)
    else if(c -> pipewrite != 1 && chdir(c -> argv[1]) != 0)
    {
        fprintf(stderr, "cd %s: %s\n", c -> argv[1], strerror(errno));
        command_result = -1;
    }

    // Restoring file descriptors:
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    dup2(stdin_copy, STDIN_FILENO);
    dup2(stdout_copy, STDOUT_FILENO);
    dup2(stderr_copy, STDERR_FILENO);
}

/**
 * [build_execute description]
 * @param commandLine [command to build and execute]
 */

void build_execute(char* commandList) {
    int type;
    char* token;
        
    // ... | command
    if(check_previous == LOGICAL_OR)
    {
        if(command_result == 0) 
        {
            // SUCCESS || command
            // the rest of the command is not interesting anymore
             commandList = NULL;
	     check_previous = 0;
	} 
    }

    // ... && command
    if(check_previous == LOGICAL_AND)
    {
        if(command_result != 0) 
        {
            // FAIL && command
	    commandList = NULL;
            check_previous = 0;
        }
    }
    
    // build the command
    command* c = command_alloc();
    c->isParent = isParent;

    while ((commandList = parse_shell_token(commandList, &type, &token)) != NULL)
    {
        command_append_arg(c, token);

	// End of command group
	if(type == TOKEN_CONTROL)   // 0
        {
            switch(*token)
            {
                case '&': 
                    c -> background = 1;
                    c -> argc--;
                    c -> argv[ c -> argc ] = NULL;
		    
		    if (c -> argc)
                        eval_command(c);
                    return;
		    
                case ';':
                    c -> background = 0;
                    c -> argc--;
                    c -> argv[ c -> argc ] = NULL;
                    
                    if (c -> argc)
                        eval_command(c);
		      
                    return;
            }
        } // end if TOKEN_CONTROL


        // ... | command		
        if(pipeused)
        {
            c -> piperead = 1;
            pipeused = 0;
        }
	
        // All the behaviour of the future excutable is set here:
        if(type == TOKEN_REDIRECTION)   // 2
        {
            switch(*token)
            {
                case '>':
                    c -> argc--;
                    c -> output_redirected = c -> argc;
                    break;
    
                case '<':
                    c -> argc--;
                    c -> input_redirected = c -> argc;
                    break;  
            }

            if(strcmp(token, "2>") == 0 )
            {
                c -> argc--;
                c -> stderr_redirected = c -> argc;
            }
        } // end if TOKEN_REDIRECTION

	if(type == TOKEN_LOGICAL)   // 3
        {
            if( strcmp(token, "||") == 0)
            {
	        
	        check_previous = LOGICAL_OR;
                c -> argc--;
                c -> argv[ c -> argc ] = NULL;
		if (c -> argc)
		  eval_command(c);
		      
		return;
	    } 

            if( strcmp(token, "&&") == 0)
            {
                
                check_previous = LOGICAL_AND;
                c -> argc--;
                c -> argv[ c -> argc ] = NULL;
		
		if (c -> argc)
                        eval_command(c);
		      
                    return;
	    } 
        } // end TOKEN_LOGICAL

	// Handle pipes
        if(type == TOKEN_CONTROL)   // 0
        {
            switch(*token)
            {
                case '|':
                {
                    c -> pipewrite = 1;
                    c -> argc--;
                    c -> argv[ c -> argc ] = NULL;
                    pipeused = 1;
            
		    // If the command will just write to a pipe
                    // (not read and write) make current command last command pointer
                    if (c->piperead != 1)
                        lc = (char*)c;
                    
		    if (c -> argc)
                        eval_command(c);
    
                    return;
                }
		
            }
        } // end if TOKEN_CONTROL
    } //end while

    // execute the command
    if (c -> argc)
        eval_command(c);
    
    // If command does not write to a pipe... free it. 
    if (c->pipewrite != 1)
        command_free(c);
}


/**
 * [eval_command_line description]
 * @param s [description]
 */
void eval_command_line(const char* s) {

    // Iterate through s string
    //endCommands = 0;  
    int start = 0;
    int length = strlen(s);
    int insideParenthesis = 0;
    for (int i = 0; i < length; i++)
    {
        // Check if we are inside of parenthesis
        if (s[i] == '"') 
        {
            insideParenthesis ++;
            if (insideParenthesis == 2) 
                insideParenthesis = 0;
        }

        // If we are not inside of a parenthesis and 
        // it is separated by ; or & or | , but not || or &&
	// checks also if user has not entered crtl+c with endCommands
	if ((insideParenthesis != 1) && (endCommands == 0) &&
               (s[i] == ';' 
                   || ( s[i] == '&' && s[i + 1] != '&') 
                   || ( s[i] == '|' && s[i + 1] != '|') 
               ) ) 
        {
            // Create command list from the start of last command list
            char *commandList = (char*) malloc(i - start + 2);
            strncpy(commandList, s + start, i - start + 1);
        		
            // build and execute command list
	    build_execute(commandList);
            free(commandList);
            start = i + 1;
	    
	    // If the line ends with a ; or a &...
	    // prepare the next command to be the parent of the command group
	    if (s[i] == ';' || ( s[i] == '&' && s[i - 1] != '&' && s[i + 1] != '&')) 
	    {
	        isParent = 1;
	    }
	    else 
	    {
	        isParent = 0;
	    }
        }
    }

    // Create and execute the last comand list
    // checks also if user has not entered crtl+c with endCommands
    if (endCommands == 0) 
    {
        char* commandList = (char*) malloc(length - start + 2);
	strncpy(commandList, s + start, length - start + 1);
	build_execute(commandList);
	free(commandList);
    }
    
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

// Signal handler to handle ctrl+c
void signal_handler() 
{
    // Stop all pending commands
    check_previous = 0;
    endCommands = 1;
    
    // Prints a prompt if crtl+c was pressed while no instruction was running
    if (bufpos == 0) { 
        printf("\nsh61[%d]$ ", getpid());
	fflush(stdout);
	needprompt = 0;
    }

    // kill all processes in group in in the main loop
    interrupted = 1;
    
}


int main(int argc, char* argv[]) {
	FILE* command_file = stdin;
	int quiet = 0;
	int r = 0;
	
	// Signal handler for ctrl+c
	signal(SIGINT,signal_handler); 
	
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
	bufpos = 0;
	needprompt = 1;
	
	while (!feof(command_file)) {

	        

		// Print the prompt at the beginning of the line
		if (needprompt && !quiet) {
			printf("sh61[%d]$ ", getpid());
			fflush(stdout);
			needprompt = 0;
		}
		int myvariable;

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
		        
		        // reset endCommand at the start of a new list
		        endCommands = 0;
			
			eval_command_line(buf);
			bufpos = 0;
			needprompt = 1;
		}

		// Kill all processes in the group of the current foreground command
		if (interrupted == 1) {
		    
		    if (currentPid > 0) 
		    {
		        kill(- currentPid, SIGKILL);
		    }
		    
		    interrupted = 0;
		}
		
		// Reap all zombie processes
		waitpid(-1, 0, WNOHANG);
	}

	return 0;
}

