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

const char* parse_shell_token(const char* str, int* type, char** token);


// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;                   // number of arguments
    char** argv;                // arguments, terminated by NULL
};


// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    for (int i = 0; i != c->argc; ++i)
        free(c->argv[i]);
    free(c->argv);
    free(c);
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND PARSING

typedef struct buildstring {
    char* s;
    int length;
    int capacity;
} buildstring;

// buildstring_append(bstr, ch)
//    Add `ch` to the end of the dynamically-allocated string `bstr->s`.

void buildstring_append(buildstring* bstr, int ch) {
    if (bstr->length == bstr->capacity) {
        int new_capacity = bstr->capacity ? bstr->capacity * 2 : 32;
        bstr->s = (char*) realloc(bstr->s, new_capacity);
        bstr->capacity = new_capacity;
    }
    bstr->s[bstr->length] = ch;
    ++bstr->length;
}

// isshellspecial(ch)
//    Test if `ch` is a command that's special to the shell (that ends
//    a command word).

static inline int isshellspecial(int ch) {
    return ch == '<' || ch == '>' || ch == '&' || ch == '|' || ch == ';'
        || ch == '(' || ch == ')' || ch == '#';
}

// parse_shell_token(str, type, token)

const char* parse_shell_token(const char* str, int* type, char** token) {
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
    buildstring_append(&buildtoken, '\0'); // terminating NUL character
    *token = buildtoken.s;
    return str;
}


// COMMAND EVALUATION

// set_foreground(p)
//    Tell the operating system that `p` is the current foreground process.
int set_foreground(pid_t p);

void eval_command(command* c) {
    pid_t pid = -1;             // process ID for child
    // Your code here!
    printf("eval_command not done yet\n");
}


void eval_command_line(const char* s) {
    int type;
    char* token;
    // Your code here!

    // build the command
    command* c = command_alloc();
    while ((s = parse_shell_token(s, &type, &token)) != NULL)
        command_append_arg(c, token);

    // execute the command
    if (c->argc)
        eval_command(c);
    command_free(c);
}


// set_foreground(p)
//    Tell the operating system that `p` is the current foreground process
//    for this terminal. This engages some ugly Unix warts, so we provide
//    it for you.
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
