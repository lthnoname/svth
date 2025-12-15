#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_LINE 80 /* Độ dài tối đa của lệnh */
#define HISTORY_SIZE 10

// Biến toàn cục để lưu lịch sử
char *history[HISTORY_SIZE];
int history_count = 0;

// Hàm thêm lệnh vào lịch sử
void add_to_history(char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) return;
    if (history_count < HISTORY_SIZE) {
        history[history_count++] = strdup(cmd);
    } else {
        free(history[0]);
        for (int i = 0; i < HISTORY_SIZE - 1; i++) {
            history[i] = history[i+1];
        }
        history[HISTORY_SIZE - 1] = strdup(cmd);
    }
}

// Hàm hiển thị lịch sử (Tính năng 2)
void print_history() {
    printf("History of commands:\n");
    for (int i = 0; i < history_count; i++) {
        printf("%d. %s\n", i + 1, history[i]);
    }
}

// Hàm xử lý tín hiệu Ctrl+C (Tính năng 5)
void handle_sigint(int sig) {
    printf("\nCaught signal %d. Type 'exit' to quit.\n", sig);
    printf("it007sh> ");
    fflush(stdout);
}

int main(void) {
    char *args[MAX_LINE/2 + 1]; /* Mảng chứa các đối số dòng lệnh */
    char input_buffer[MAX_LINE]; /* Bộ đệm chứa lệnh người dùng nhập */
    int should_run = 1;

    // Đăng ký hàm xử lý tín hiệu SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    while (should_run) {
        printf("it007sh> ");
        fflush(stdout);

        // Đọc lệnh từ người dùng
        if (fgets(input_buffer, MAX_LINE, stdin) == NULL) {
            break; // Thoát nếu gặp EOF
        }

        // Loại bỏ ký tự xuống dòng ở cuối
        input_buffer[strcspn(input_buffer, "\n")] = 0;

        // Nếu người dùng nhập "exit", thoát chương trình
        if (strcmp(input_buffer, "exit") == 0) {
            should_run = 0;
            continue;
        }

        // Tính năng 2: Nếu nhập HF, hiển thị lịch sử và bỏ qua thực thi
        if (strcmp(input_buffer, "HF") == 0) {
            print_history();
            continue;
        }

        // Lưu lệnh vào lịch sử
        add_to_history(input_buffer);

        // Xử lý chuỗi nhập vào để tách thành các arguments
        // Kiểm tra xem có Pipe "|" không (Tính năng 4)
        char *pipe_pos = strchr(input_buffer, '|');
        
        if (pipe_pos != NULL) {
            *pipe_pos = 0; // Tách chuỗi thành 2 phần
            char *cmd1 = input_buffer;
            char *cmd2 = pipe_pos + 1;

            // Tách arg cho cmd1
            char *args1[MAX_LINE/2 + 1];
            int i = 0;
            args1[i] = strtok(cmd1, " ");
            while (args1[i] != NULL) {
                i++;
                args1[i] = strtok(NULL, " ");
            }

            // Tách arg cho cmd2
            char *args2[MAX_LINE/2 + 1];
            int j = 0;
            args2[j] = strtok(cmd2, " ");
            while (args2[j] != NULL) {
                j++;
                args2[j] = strtok(NULL, " ");
            }

            // Tạo Pipe
            int fd[2];
            pipe(fd);

            if (fork() == 0) {
                // Child 1: Chạy cmd1, output vào pipe
                dup2(fd[1], STDOUT_FILENO);
                close(fd[0]);
                close(fd[1]);
                execvp(args1[0], args1);
                exit(1);
            }

            if (fork() == 0) {
                // Child 2: Chạy cmd2, input từ pipe
                dup2(fd[0], STDIN_FILENO);
                close(fd[1]);
                close(fd[0]);
                execvp(args2[0], args2);
                exit(1);
            }

            // Parent đóng pipe và chờ
            close(fd[0]);
            close(fd[1]);
            wait(NULL);
            wait(NULL);

        } else {
            // Trường hợp không có Pipe (Chạy lệnh thường hoặc Redirection)
            int i = 0;
            args[i] = strtok(input_buffer, " ");
            while (args[i] != NULL) {
                i++;
                args[i] = strtok(NULL, " ");
            }

            if (args[0] == NULL) continue; // Lệnh rỗng

            pid_t pid = fork(); // Tạo tiến trình con (Tính năng 1)

            if (pid < 0) {
                fprintf(stderr, "Fork failed");
                return 1;
            } else if (pid == 0) {
                // --- Tiến trình con ---
                
                // Tính năng 3: Redirection (>, <)
                for (int k = 0; args[k] != NULL; k++) {
                    // Output redirection ">"
                    if (strcmp(args[k], ">") == 0) {
                        args[k] = NULL; // Cắt lệnh tại dấu >
                        int fd_out = open(args[k+1], O_CREAT | O_WRONLY | O_TRUNC, 0644);
                        if (fd_out < 0) { perror("Open failed"); exit(1); }
                        dup2(fd_out, STDOUT_FILENO);
                        close(fd_out);
                    } 
                    // Input redirection "<"
                    else if (strcmp(args[k], "<") == 0) {
                        args[k] = NULL; // Cắt lệnh tại dấu <
                        int fd_in = open(args[k+1], O_RDONLY);
                        if (fd_in < 0) { perror("Open failed"); exit(1); }
                        dup2(fd_in, STDIN_FILENO);
                        close(fd_in);
                    }
                }

                // Thực thi lệnh
                if (execvp(args[0], args) == -1) {
                    printf("Command not found\n");
                }
                exit(0);
            } else {
                // --- Tiến trình cha ---
                wait(NULL); // Chờ tiến trình con kết thúc
            }
        }
    }
    return 0;
}