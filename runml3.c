#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS
    #include <windows.h>
#else
    #define OS_UNIX
#endif

#define MAX_LINE_LENGTH 1000
#define MAX_IDENTIFIER_LENGTH 12
#define MAX_WORDS 20
#define COMMAND_BUFFER_SIZE 512

int get_indent_level(const char *line) {
    if (*line == '\t') return 1;
    return 0;
}

void split_line_into_words(char line[], char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int *word_count, char *indent) {
    const char *line_ptr = line;
    while (*line_ptr == '\t' || *line_ptr == ' ') {
        *indent++ = *line_ptr++;
    }
    *indent = '\0';

    char temp_line[MAX_LINE_LENGTH];
    strcpy(temp_line, line_ptr);

    char *token = strtok(temp_line, " \t\n");
    *word_count = 0;

    while (token != NULL && *word_count < MAX_WORDS) {
        strncpy(words[*word_count], token, MAX_IDENTIFIER_LENGTH);
        words[*word_count][MAX_IDENTIFIER_LENGTH] = '\0';
        (*word_count)++;
        token = strtok(NULL, " \t\n");
    }
}

void handle_function_definition(FILE *output_file, char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int word_count) {
    fprintf(output_file, "    double %s(", words[1]);
    for (int i = 2; i < word_count; i++) {
        if (i > 2) fprintf(output_file, ", ");
        fprintf(output_file, "double %s", words[i]);
    }
    fprintf(output_file, ") {\n");
}

void close_function_definition(FILE *output_file, int *in_function) {
    if (*in_function == 1) {
        fprintf(output_file, "    }\n");
        *in_function = 0;
    }
}

void handle_print_statement(FILE *output_file, char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int word_count) {
    fprintf(output_file, "    printf(\"%%.6f\\n\", ");
    for (int i = 1; i < word_count; i++) {
        fprintf(output_file, "%s", words[i]);
        if (i < word_count - 1) {
            fprintf(output_file, " ");
        }
    }
    fprintf(output_file, ");\n");
}

void handle_assignment(FILE *output_file, char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int word_count) {
    fprintf(output_file, "    double %s = ", words[0]);
    for (int i = 2; i < word_count; i++) {
        fprintf(output_file, "%s", words[i]);
        if (i < word_count - 1) {
            fprintf(output_file, " ");
        }
    }
    fprintf(output_file, ";\n");
}

void handle_expression(FILE *output_file, char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int word_count) {
    fprintf(output_file, "    ");
    for (int i = 0; i < word_count; i++) {
        fprintf(output_file, "%s", words[i]);
        if (i < word_count - 1) {
            fprintf(output_file, " ");
        }
    }
    fprintf(output_file, ";\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2 || !strrchr(argv[1], '.') || strcmp(strrchr(argv[1], '.'), ".ml") != 0) {
        fprintf(stderr, "Usage: %s <input_file.ml>\n", argv[0]);
        return 1;
    }

    FILE *input_file = fopen(argv[1], "r");
    if (!input_file) {
        fprintf(stderr, "Error opening file %s\n", argv[1]);
        return 1;
    }

    char c_filename[256];
    snprintf(c_filename, sizeof(c_filename), "ml-%d.c", getpid());
    FILE *output_file = fopen(c_filename, "w");
    if (!output_file) {
        fprintf(stderr, "Error creating output file %s\n", c_filename);
        fclose(input_file);
        return 1;
    }

    // Write the necessary headers for the C file and main function
    fprintf(output_file, "#include <stdio.h>\n#include <math.h>\n\nint main() {\n");

    int prev_ind = 0;
    int curr_ind = 0;
    int in_function = 0;
    char line[MAX_LINE_LENGTH];
    char line_copy[MAX_LINE_LENGTH];
    char indent[MAX_LINE_LENGTH];
    char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1];
    int word_count;

    while (fgets(line, sizeof(line), input_file)) {
        strcpy(line_copy, line);
        curr_ind = get_indent_level(line);

        split_line_into_words(line_copy, words, &word_count, indent);

        if (word_count == 0 || words[0][0] == '#') {
            continue;
        }

        if (strcmp(words[0], "function") == 0 && curr_ind == 0) {
            if (in_function) {
                close_function_definition(output_file, &in_function);
            }
            handle_function_definition(output_file, words, word_count);
            in_function = 1;
            prev_ind = curr_ind;
            continue;
        } else if (in_function && curr_ind == 0 && prev_ind == 1) {
            close_function_definition(output_file, &in_function);
        } else if (in_function) {
            fprintf(output_file, "    ");
        }

        if (strcmp(words[0], "return") == 0) {
            handle_expression(output_file, words, word_count); // Handles the return statement
        } else if (strcmp(words[0], "print") == 0) {
            handle_print_statement(output_file, words, word_count);
        } else if (word_count >= 3 && strcmp(words[1], "<-") == 0) {
            handle_assignment(output_file, words, word_count);
        } else {
            handle_expression(output_file, words, word_count);
        }

        prev_ind = curr_ind;
    }

    if (in_function) {
        close_function_definition(output_file, &in_function);
    }

    // End the main function
    fprintf(output_file, "    return 0;\n}\n");

    fclose(input_file);
    fclose(output_file);

    // Compilation of the generated C file
    char compile_command[COMMAND_BUFFER_SIZE];
    char exec_filename[COMMAND_BUFFER_SIZE];
    snprintf(exec_filename, sizeof(exec_filename), "ml-%d", getpid());

#ifdef OS_WINDOWS
    snprintf(compile_command, sizeof(compile_command), "gcc -std=c11 -Wall -Werror -o %s.exe %s", exec_filename, c_filename);
    char run_command[COMMAND_BUFFER_SIZE];
    snprintf(run_command, sizeof(run_command), "%s.exe > output-%d.txt", exec_filename, getpid());
#else
    snprintf(compile_command, sizeof(compile_command), "gcc -std=c11 -Wall -Werror -o %s %s", exec_filename, c_filename);
    char run_command[COMMAND_BUFFER_SIZE];
    snprintf(run_command, sizeof(run_command), "./%s > output-%d.txt", exec_filename, getpid());
#endif

    if (system(compile_command) != 0) {
        fprintf(stderr, "! Compilation failed\n");
        return 1;
    }

    // Running the compiled program and redirecting output to a file
    if (system(run_command) != 0) {
        fprintf(stderr, "! Execution failed\n");
        return 1;
    }

    // Processing the output to truncate decimals if they are exact floats
    char output_filename[COMMAND_BUFFER_SIZE];
    snprintf(output_filename, sizeof(output_filename), "output-%d.txt", getpid());

    FILE *output_file_read = fopen(output_filename, "r");
    if (!output_file_read) {
        fprintf(stderr, "Error opening output file %s\n", output_filename);
        return 1;
    }

    char output_line[MAX_LINE_LENGTH];
    while (fgets(output_line, sizeof(output_line), output_file_read) != NULL) {
        double value;
        if (sscanf(output_line, "%lf", &value) == 1) {
            if (value == (int)value) {
                printf("%d\n", (int)value);
            } else {
                printf("%.6f\n", value);
            }
        } else {
            printf("%s", output_line);
        }
    }
    fclose(output_file_read);

    // Remove the compiled executable and the .c file
#ifdef OS_WINDOWS
    char remove_command[COMMAND_BUFFER_SIZE];
    snprintf(remove_command, sizeof(remove_command), "del %s.exe %s", exec_filename, c_filename);
#else
    char remove_command[COMMAND_BUFFER_SIZE];
    snprintf(remove_command, sizeof(remove_command), "rm %s %s", exec_filename, c_filename);
#endif
    system(remove_command);

    return 0;
}
