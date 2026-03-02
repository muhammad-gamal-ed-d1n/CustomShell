#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#define MAX_ARGS 10

char **parse_input(char *input);
int execute_command(char** args);

int set_environment(char *cwd)
{
    getcwd(cwd, sizeof(cwd));
}

int shell_loop(char **env)
{
    // initialize variables
    char *input = NULL;
    size_t input_size = 0;
    char *cwd = getcwd(NULL, 0);

    // set wokring environment
    set_environment(cwd);

    while (1)
    {
        // prompt for input
        printf("user@%s$ ", cwd);
        if (getline(&input, &input_size, stdin) == -1)
        {
            perror("Error reading input.");
            exit(1);
        }
        char** args = parse_input(input);

        execute_command(args);
    }
}

int execute_command(char** args) {
    __pid_t pid = fork();

    if (pid == 0) {
        execvp(args[0], args);
        perror("execvp error");
        exit(1);
    }
    else {
        int status = 0;
        pid_t end = waitpid(pid, &status, 0);
        return 0;
    }
}

int main(char **env)
{
    shell_loop(env);
}

char **parse_input(char *input)
{
    char **tokens = malloc(MAX_ARGS * sizeof(char *));
    char *token = NULL;
    size_t position = 0, token_len = 0;

    if (!tokens)
    {
        perror("Error parsing input");
        exit(0);
    }

    for (int i = 0; input[i];)
    {

        token_len = 0;
        token = NULL;
        while (input[i] && input[i] != ' ' && input[i] != '\n')
        {
            token_len++;
            i++;
        }

        tokens[position] = malloc(sizeof(char) * (token_len + 1));

        if (!tokens[position]) {
            perror("Error parsing input");
            exit(1);
        }

        size_t start = i - token_len;
        for (int j = 0; j < token_len; j++) {
            tokens[position][j] = input[start + j];
        }
        tokens[position][token_len] = '\0';
        position++;

        while(input[i] && (input[i] == ' ' || input[i] == '\n')) {
            i++;
        }
    }

    tokens[position] = NULL;
    return tokens;
}