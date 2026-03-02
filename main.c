#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include "lib/hashmap.h"
#include <string.h>

#define MAX_ARGS 10
#define MAX_PATH 1024

typedef int (*builtin_func)(char **args, struct hashmap *env);

struct builtin
{
    char *name;
    builtin_func f;
};

struct var
{
    char *name;
    char *value;
};

// prototypes
char **parse_input(char *input);
int execute_command(char **args);
int free_args(char **args);
uint64_t builtin_hash(const void *item, uint64_t seed0, uint64_t seed1);
int builtin_compare(const void *a, const void *b, void *udata);
bool builtin_iter(const void *item, void *udata);
int initialize_builtins(struct hashmap *builtins);
int var_compare(const void *a, const void *b, void *udata);
uint64_t var_hash(const void *item, uint64_t seed0, uint64_t seed1);
int cd(char **args, struct hashmap *env);
int echo(char **args, struct hashmap *env);
int export(char **args, struct hashmap *env);

int set_environment(char *cwd)
{
    getcwd(cwd, sizeof(cwd));
}

int shell_loop()
{
    // initialize variables
    char *input = NULL;
    size_t input_size = 0;
    char *cwd;
    struct hashmap *builtins = hashmap_new(
        sizeof(struct builtin),
        0, 0, 0,
        builtin_hash, builtin_compare,
        NULL, NULL);
    struct hashmap *env = hashmap_new(
        sizeof(struct var),
        0, 0, 0,
        var_hash, var_compare,
        NULL, NULL);

    initialize_builtins(builtins);

    while (1)
    {
        // set wokring environment
        cwd = getcwd(NULL, 0);

        // prompt for input
        printf("user@%s$ ", cwd);
        if (getline(&input, &input_size, stdin) == -1)
        {
            perror("Error reading input.");
            exit(1);
        }
        char **args = parse_input(input);

        const struct builtin *builtin = hashmap_get(builtins, &(struct builtin){.name = args[0]});
        if (builtin)
        {
            builtin->f(args, env);
        }
        else
        {
            execute_command(args);
        }
        free_args(args);
    }
}

int execute_command(char **args)
{
    __pid_t pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        return 1;
    }
    if (pid == 0)
    {
        execvp(args[0], args);
        perror("execvp error");
        exit(1);
    }
    else
    {
        int status = 0;
        pid_t end = waitpid(pid, &status, 0);
        return 0;
    }
}

int free_args(char **args)
{
    for (int i = 0; args[i]; i++)
    {
        free(args[i]);
    }
    free(args);
}

int main(void)
{
    shell_loop();
}

char **parse_input(char *input)
{
    char **tokens = malloc(MAX_ARGS * sizeof(char *));
    size_t position = 0, token_len = 0;
    int quoted = 0;

    if (!tokens)
    {
        perror("Error parsing input");
        exit(0);
    }

    for (int i = 0; input[i];)
    {

        token_len = 0;
        quoted = 0;
        // loop over normal input with no quotes
        while (input[i] && input[i] != ' ' && input[i] != '"' && input[i] != '\n')
        {
            token_len++;
            i++;
        }

        // handle quoted input
        if (input[i] == '"')
        {
            quoted = 1;
            i++;
            while (input[i] && input[i] != '"' && input[i] != '\n')
            {
                token_len++;
                i++;
            }
            if (input[i] != '"')
            {
                printf("Non-terminated quotation\n");
                exit(1);
            }
            i++;
        }

        tokens[position] = malloc(sizeof(char) * (token_len + 1));

        if (!tokens[position])
        {
            perror("Error parsing input");
            exit(1);
        }

        size_t start = i - token_len - quoted;
        for (int j = 0; j < token_len; j++)
        {
            tokens[position][j] = input[start + j];
        }
        tokens[position][token_len] = '\0';
        position++;

        while (input[i] && (input[i] == ' ' || input[i] == '\n'))
        {
            i++;
        }
    }

    tokens[position] = NULL;
    return tokens;
}

// functions required by the hashmap
int builtin_compare(const void *a, const void *b, void *udata)
{
    const struct builtin *ua = a;
    const struct builtin *ub = b;
    return strcmp(ua->name, ub->name);
}

uint64_t builtin_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct builtin *builtin = item;
    return hashmap_sip(builtin->name, strlen(builtin->name), seed0, seed1);
}

int var_compare(const void *a, const void *b, void *udata)
{
    const struct var *ua = a;
    const struct var *ub = b;
    return strcmp(ua->name, ub->name);
}

uint64_t var_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct var *var = item;
    return hashmap_sip(var->name, strlen(var->name), seed0, seed1);
}

// initialize builtin map
int initialize_builtins(struct hashmap *builtins)
{
    hashmap_set(builtins, &(struct builtin){.name = "cd", .f = cd});
    hashmap_set(builtins, &(struct builtin){.name = "echo", .f = echo});
    hashmap_set(builtins, &(struct builtin){.name = "cd", .f = cd});
}

// builtin functions
int cd(char **args, struct hashmap *env)
{
    char *path = args[1];
    char *fullpath = malloc(MAX_PATH * sizeof(char));
    if (!path)
    {
        printf("Usage: cd PATH\n");
        return 0;
    }
    if (path[0] == '~')
    {
        const char *home = getenv("HOME");
        snprintf(fullpath, sizeof(fullpath), "%s%s", home, path + 1);
        printf("%s\n", fullpath);
        printf("%s\n", path);
    }
    if (chdir(path) != 0)
    {
        perror("Error:");
        return 0;
    }
}

int echo(char **args, struct hashmap *env)
{
    // iterate over words
    for (int i = 1; args[i]; i++)
    {
        if (args[i][0] == '$')
        {
            char *name = strdup(args[i] + 1);
            const struct var *v = hashmap_get(env, &(struct var){.name = name});
            if (v) {
                printf("%s", v->name);
            }
        }
        else
        {
            printf("%s", args[i]);
        }
        if (args[i + 1])
        {
            printf(" ");
        }
    }
    printf("\n");
}

int export(char **args, struct hashmap *env)
{
}