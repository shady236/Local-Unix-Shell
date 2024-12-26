#include "shell.h"

#define  ESC   (27) 
#define  DEL   (127) 


static void display_clear(int num_chars) {
    printf("\033[0m");
    for (int i = 0; i < num_chars; i++) {
        printf("\b");
    }
    for (int i = 0; i < num_chars; i++) {
        printf(" ");
    }
    for (int i = 0; i < num_chars; i++) {
        printf("\b");
    }
}


static void display_cmd(char *cmd, int cursor, bool newline) {
    printf("\033[0m");
    int i;
    for (i = 0; cmd[i]; i++) {
        if (i == cursor && !newline) {
            printf("\033[1;47;30m");
        }
        printf("%c", cmd[i]);
        if (i == cursor) {
            printf("\033[0m");
        }
    }

    if (cursor == i && !newline) {
        printf("\033[1;47;30m");
    }
    printf(" \033[0m");
    if (newline) {
        printf("\r\n");
    }
}


static void replace_substring(char **str, int from, int to, char *new_substring) {
    int old_substring_len = to - from + 1;
    int new_substring_len = strlen(new_substring);
    
    int old_str_len = strlen(*str);
    int new_str_len = old_str_len - old_substring_len + new_substring_len;
    
    if (new_str_len > old_str_len) {
        *str = realloc(*str, (new_str_len + 1) * sizeof(char));
    } 

    // strcat(*str, *str + to + 1);
    strcpy(*str + from + new_substring_len, *str + to + 1);
    strncpy(*str + from, new_substring, new_substring_len);
}


static void execute_external_cmd(command **cmd, int i) {
    int pipe_fd[2] = {-1, -1};
    if (cmd[i]->pipe_to != 0 && cmd[i]->redirect_out == NULL) {
        if (pipe(pipe_fd) == -1) {
            perror("pipe error\n");
            exit(EXIT_FAILURE);
        }
        cmd[cmd[i]->pipe_to]->pipe_fd[0] = pipe_fd[0];
        cmd[i]->pipe_fd[1] = pipe_fd[1];
    }

    pid_t pid = fork();
    if (pid == -1) {
        // Error 
    }
    else if (pid != 0) {                        // Parent process
        close(pipe_fd[1]);
        close(cmd[i]->pipe_fd[0]);

        if (!cmd[i]->background) {
            waitpid(pid, NULL, 0);
        }

        int status;
        while (true) {
            pid_t pid = waitpid(-1, &status, WNOHANG);
            if (pid <= 0) {
                break;
            }
        }
    }
    else {
        if (cmd[i]->redirect_in != NULL) {
            int fd = open(cmd[i]->redirect_in, O_RDONLY);
            if (fd == -1) {
                return;
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        else if (cmd[i]->pipe_fd[0] != -1) {
            dup2(cmd[i]->pipe_fd[0], STDIN_FILENO);
            close(cmd[i]->pipe_fd[0]);
        }
        
        if (cmd[i]->redirect_out != NULL) {
            int fd = open(cmd[i]->redirect_out, O_CREAT | O_WRONLY | O_TRUNC, 0664);          // O_TRUNC for clearing the previous file contents
            if (fd == -1) {
                return;
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        else if (cmd[i]->pipe_fd[1] != -1) {
            dup2(cmd[i]->pipe_fd[1], STDOUT_FILENO);
            close(cmd[i]->pipe_fd[1]);
        }

        if (cmd[i]->redirect_err != NULL) {
            int fd = open(cmd[i]->redirect_err, O_CREAT | O_WRONLY | O_TRUNC, 0664);          // O_TRUNC for clearing the previous file contents
            if (fd == -1) {
                return;
            }
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        close(pipe_fd[0]);
        close(pipe_fd[1]);
        close(cmd[i]->pipe_fd[0]);
        close(cmd[i]->pipe_fd[1]);
        execvp(cmd[i]->com_name, cmd[i]->argv);
        fprintf(stderr, "execvp error\n");
        exit(EXIT_FAILURE);
    }
}


static bool is_builtin_cmd(command *cmd) {
    return (
        (strcmp(cmd->com_name, "prompt") == 0) || 
        (strcmp(cmd->com_name, "pwd") == 0) || 
        (strcmp(cmd->com_name, "cd") == 0) || 
        (strcmp(cmd->com_name, "history") == 0) 
    );
}

static void execute_builtin_cmd(command **cl, int i, char **prompt, char **history, int history_len, int history_start_idx) {
    command *cmd = cl[i];
    int pipe_fd[2] = {-1, -1};
    if (cl[i]->pipe_to != 0 && cl[i]->redirect_out == NULL) {
        if (pipe(pipe_fd) == -1) {
            perror("pipe error\n");
            exit(EXIT_FAILURE);
        }
        cl[cl[i]->pipe_to]->pipe_fd[0] = pipe_fd[0];
        cl[i]->pipe_fd[1] = pipe_fd[1];
    }

    if (strcmp(cmd->com_name, "prompt") == 0) {
        if (cmd->argc == 2) {
            char *new_prompt = strdup(cmd->argv[1]);
            if (new_prompt == NULL) {
                fprintf(stderr, "NEW_PROMPT: \"%s\" is too long. Can't allocate memory for it\n", cmd->argv[1]);
            }
            else {
                free(*prompt);
                *prompt = new_prompt;
            }
        }
        else {
            perror("Invalid number of arguments\n");
            perror("Usage: prompt NEW_PROMPT [2> ERROR_FILE]\n");
        }
    }
    else if (strcmp(cmd->com_name, "pwd") == 0) {
        if (cmd->argc == 1) {
            char *cwd = get_current_dir_name();
            if (cwd == NULL) {
                perror("getcwd failed to allocate memory\n");
            }
            else if (cmd->redirect_out != NULL) {
                FILE* f = fopen(cmd->redirect_out, "w");
                fprintf(f, "%s\n", cwd);
                fclose(f);
            }
            else if (cmd->pipe_to != 0) {
                write(cmd->pipe_fd[1], cwd, strlen(cwd));
                write(cmd->pipe_fd[1], "\n", strlen("\n"));
            }
            else {
                printf("%s\n", cwd);
            }

            if (cwd != NULL) {
                free(cwd);
            }
        }
        else {
            perror("Invalid number of arguments\n");
            perror("Usage: pwd [> OUTPUT_FILE] [2> ERROR_FILE]\n");
        }
    }
    else if (strcmp(cmd->com_name, "cd") == 0) {
        if (cmd->argc == 1) {
            if (chdir((getpwuid(getuid())->pw_dir)) == -1) {
                perror("chdir to HOME failed\n");
            }
        }
        else if (cmd->argc == 2) {
            if (chdir(cmd->argv[1]) == -1) {
                fprintf(stderr, "%s is NOT a valid directory \n", cmd->argv[1]);
            }
        }
        else {
            perror("Invalid number of arguments\n");
            perror("Usage: cd [NEW_DIRECTORY] [2> ERROR_FILE]\n");
        }
    }
    else if (strcmp(cmd->com_name, "history") == 0) {
        if (cmd->argc == 1) {
            int fd;
            if (cmd->redirect_out != NULL) {
                fd = open(cmd->redirect_out, O_WRONLY | O_CREAT | O_TRUNC, 0664);
            }
            else if (cmd->pipe_to != 0) {
                fd = cmd->pipe_fd[1];
            }
            else {
                fd = STDOUT_FILENO;
            }

            for (int i = 0; i < history_len; i++) {
                char line_num[15];
                sprintf(line_num, "%5d\t", i);
                write(fd, line_num, strlen(line_num));
                write(fd, history[(history_start_idx + i) % HISTORY_SIZE], strlen(history[(history_start_idx + i) % HISTORY_SIZE]));
                write(fd, "\n", strlen("\n"));
            }

            if (cmd->redirect_out != NULL) {
                close(fd);
            }
        }
        else {
            perror("Invalid number of arguments\n");
            perror("Usage: history [> OUTPUT_FILE] [2> ERROR_FILE]\n");
        }
    }

    if (cmd->pipe_fd[0] != -1) {
        close(cmd->pipe_fd[0]);
    }
    if (cmd->pipe_fd[1] != -1) {
        close(cmd->pipe_fd[1]);
    }
}


void ignore_sig(int sig) {
    if (sig == SIGQUIT) {               // CTRL-\ signal

    }
    else if (sig == SIGINT) {           // CTRL-C signal
#ifdef DEBUG
        exit(0);
#endif // DEBUG
    }
    else if (sig == SIGTSTP) {          // CTRL-Z signal

    }
}


void disable_echo() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}


void enable_echo() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag |= ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}


char *get_user_cmd_str(char *prompt, char **history, int history_start_idx, int history_len) {
    char ch;
    int cmd_cap = 100;
    int cmd_len = 0;
    int displayed_cmd_len = 0;
    int cmd_cursor_idx = 0;
    int history_idx = -1;
    char *cmd = malloc((cmd_cap + 1) * sizeof(char));
    cmd[0] = '\0';

    struct termios org_termios;
    tcgetattr(STDIN_FILENO, &org_termios);
    
    struct termios raw_termios = org_termios;
    cfmakeraw(&raw_termios);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
    printf("\e[?25l");          // hide terminal built-in cursor

    printf("%s  ", prompt);
    do {
        display_clear(displayed_cmd_len + 1);
        if (history_idx == -1) {
            displayed_cmd_len = cmd_len;
            display_cmd(cmd, cmd_cursor_idx, false);
        }
        else {
            displayed_cmd_len = strlen(history[(history_start_idx + history_idx) % HISTORY_SIZE]);
            display_cmd(history[(history_start_idx + history_idx) % HISTORY_SIZE], displayed_cmd_len, false);
        }

        if (cmd_len >= cmd_cap) {
            cmd_cap *= 2;
            cmd = realloc(cmd, (cmd_cap + 1) * sizeof(char));
        }

        ch = getchar();
        if (ch == ESC) {
            ch = getchar();
            if (ch != '[') {
                continue;
            }

            ch = getchar();
            if ((ch < 'A' || ch > 'D') && ch != '3') {
                ungetc(ch, stdin);
                continue;
            }

            
            if (ch == 'C') {
                if (history_idx != -1) {
                    printf("\a");
                }
                else {
                    if (cmd_cursor_idx < cmd_len) {
                        cmd_cursor_idx++;
                    }
                    else {
                        printf("\a");
                    }
                }
            } 
            else if (ch == 'D') {
                if (history_idx != -1) {
                    free(cmd);
                    cmd = strdup(history[(history_start_idx + history_idx) % HISTORY_SIZE]);
                    cmd_len = strlen(cmd);
                    cmd_cap = cmd_len;
                    cmd_cursor_idx = cmd_len;
                    history_idx = -1;
                }

                if (cmd_cursor_idx > 0) {
                    cmd_cursor_idx--;
                }
                else {
                    printf("\a");
                }
            }
            
            // TODO: navigate history ('A' for up - 'B' for down)
            else if (ch == 'A') {
                if (history_idx == -1 && history_len > 0) {
                    history_idx = history_len - 1;
                }
                else if (history_idx > 0) {
                    history_idx--;
                }
                else {
                    printf("\a");
                }
            }
            else if (ch == 'B') {
                if (history_idx == -1) {
                    printf("\a");
                }
                else if (history_idx == history_len - 1) {
                    history_idx = -1;
                }
                else {
                    history_idx++;
                }
            }

            // TODO: handle delete key
            else if (ch == '3') {
                if (history_idx != -1) {
                    ungetc(ch, stdin);
                    printf("\a");
                }
                else {
                    ch = getchar();
                    if (ch == '~') {
                        if (cmd_cursor_idx < cmd_len) {
                            for (int i = cmd_cursor_idx; i < cmd_len; i++) {
                                cmd[i] = cmd[i + 1];
                            }
                            cmd_len--;
                        }
                        else {
                            printf("\a");
                        }
                    }
                    else {
                        ungetc('~', stdin);
                        ungetc('3', stdin);
                    }
                }
            }
        }
        else if (history_idx != -1) {
            free(cmd);
            cmd = strdup(history[(history_start_idx + history_idx) % HISTORY_SIZE]);
            cmd_len = strlen(cmd);
            cmd_cap = cmd_len;
            cmd_cursor_idx = cmd_len;
            history_idx = -1;
            if (ch != '\n' && ch != '\r') {
                ungetc(ch, stdin);
            }
        }
        else if (ch == DEL) {
            if (cmd_cursor_idx > 0) {
                for (int i = cmd_cursor_idx; i <= cmd_len; i++) {
                    cmd[i - 1] = cmd[i];
                }
                cmd_len--;
                cmd_cursor_idx--;
            }
            else {
                printf("\a");
            }
        }
        else if (!iscntrl(ch)) {
            for (int i = cmd_len; i >= cmd_cursor_idx; i--) {
                cmd[i + 1] = cmd[i];
            }
            cmd[cmd_cursor_idx] = ch;
            cmd_cursor_idx++;
            cmd_len++;
        }
        else {
            printf("\a");
        }

        cmd[cmd_len] = '\0';
    } while (ch != '\n' && ch != '\r');

    display_clear(displayed_cmd_len + 1);
    display_cmd(cmd, cmd_cursor_idx, true);
    tcsetattr(STDIN_FILENO, TCSANOW, &org_termios);
    printf("\e[?25h");          // re-show terminal built-in cursor
    return cmd;
}


bool resolve_history_pointers(char **cmd, char **history, int history_start_idx, int history_len) {
    int in_qoute = 0;
    
    for (int i = 0; (*cmd)[i]; i++) {
        if ((*cmd)[i] == '"' || (*cmd)[i] == '\'') {
            in_qoute = !in_qoute;
            continue;
        }
        else if (in_qoute || (*cmd)[i] != '!') {
            continue;
        }
        else if ((*cmd)[i] == '\\') {
            if ((*cmd)[i + 1]) {
                i++;
            }
            continue;
        }
        
        char *end = strchr(*cmd + i, ' ');
        if (end == NULL) {
            end = strchr(*cmd + i, '\0');
        }

        if (isdigit((*cmd)[i + 1])) {
            int idx = atoi(*cmd + i + 1);
            if (idx >= history_len) {
                // Error
                return false;
            }

            char *prev_cmd = history[(history_start_idx + idx) % HISTORY_SIZE];
            replace_substring(cmd, i, end - *cmd - 1, prev_cmd);
        }
        else {
            char *prev_cmd = NULL;
            for (int j = history_len - 1; j >= 0; j--) {
                if (strncmp(*cmd + i + 1, history[(history_start_idx + j) % HISTORY_SIZE], end - *cmd - i - 1) == 0) {
                    prev_cmd = history[(history_start_idx + j) % HISTORY_SIZE];
                    break;
                }
            }

            if (prev_cmd == NULL) {
                return false;
            }

            replace_substring(cmd, i, end - *cmd - 1, prev_cmd);
        }
    }

    return true;
}


command **get_user_cmd(char *prompt, char **history, int *history_start_idx, int *history_len) {
    char *cmd = get_user_cmd_str(prompt, history, *history_start_idx, *history_len);

    if (!resolve_history_pointers(&cmd, history, *history_start_idx, *history_len)) {
        free(cmd);
        return NULL;
    }

    char *org_cmd_ptr = cmd;
    while (isspace(*cmd)) {
        cmd++;
    }

    if (*cmd == '\0') {
        free(org_cmd_ptr);
        return NULL;
    }

    // Add to history buffer
    history[(*history_start_idx + *history_len) % HISTORY_SIZE] = strdup(cmd);
    if (*history_len < HISTORY_SIZE) {
        (*history_len)++;
    }
    else {
        *history_start_idx = (*history_start_idx + 1) % HISTORY_SIZE;
    }

    command **ret = process_cmd_line(cmd, 1);
    free(org_cmd_ptr);
    return ret;
}


void run_shell() {
    bool exit = false;
    command **cl;

    char *prompt = malloc(2 * sizeof(char));
    strcpy(prompt, "%");

    char *history[HISTORY_SIZE] = {NULL};
    int history_start_idx = 0;
    int history_len = 0;

    signal(SIGQUIT, ignore_sig);
    signal(SIGINT, ignore_sig);
    signal(SIGTSTP, ignore_sig);
    disable_echo();
    setbuf(stdout, NULL);

    while (!exit) {
        command **cl = get_user_cmd(prompt, history, &history_start_idx, &history_len);
        if (cl == NULL) {
            continue;
        }

        for (int i = 0; cl[i] != NULL; i++) {
            command *cmd = cl[i];

            if (strcmp(cmd->com_name, "exit") == 0) {
                exit = true;
                break;
            }
            else if (is_builtin_cmd(cmd)) {
                execute_builtin_cmd(cl, i, &prompt, history, history_len, history_start_idx);
            }
            else {
                execute_external_cmd(cl, i);
            }
        }
        
        clean_up(cl);
    }

    enable_echo();
}
