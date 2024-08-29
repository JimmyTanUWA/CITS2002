#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_LINE_LENGTH 1000
#define MAX_IDENTIFIER_LENGTH 12
#define MAX_WORDS 20

void split_line_into_words(char line[], char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1], int *word_count) {
    char *token = strtok(line, " \t\n");
    *word_count = 0;
    
    while (token != NULL && *word_count < MAX_WORDS) {
        strncpy(words[*word_count], token, MAX_IDENTIFIER_LENGTH);
        words[*word_count][MAX_IDENTIFIER_LENGTH] = '\0'; // Ensure null-termination
        (*word_count)++;
        token = strtok(NULL, " \t\n");
    }
}

int get_indent_level(const char *line) {
    int indent_level = 0;
    while (*line == '\t') {
        indent_level++;
        line++;
    }
    return indent_level;
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
    
    int line_number = 0;
    int current_indent = 0;
    int in_function = 0;
    char line[MAX_LINE_LENGTH];
    char words[MAX_WORDS][MAX_IDENTIFIER_LENGTH + 1];
    int word_count;

    printf("@ Reading and processing input file...\n");

    while (fgets(line, sizeof(line), input_file)) {
        line_number++;
        printf("@ Processing line %d: %s", line_number, line);

        int indent_level = get_indent_level(line);
        split_line_into_words(line, words, &word_count);

        if (word_count == 0 || words[0][0] == '#') continue;  // Skip empty lines and comments

        if (strcmp(words[0], "function") == 0) {
            printf("@ Detected function definition at line %d\n", line_number);
            fprintf(output_file, "// Function definition: %s\n", words[1]);
            fprintf(output_file, "double %s(", words[1]);
            for (int i = 2; i < word_count; i++) {
                if (i > 2) fprintf(output_file, ", ");
                fprintf(output_file, "double %s", words[i]);
            }
            fprintf(output_file, ") {\n"); // Open the function block with '{'
            in_function = 1;
            current_indent = indent_level;
            continue;
        }

        if (indent_level <= current_indent && in_function) {
            fprintf(output_file, "}\n");
            in_function = 0;
        }

        if (strcmp(words[0], "return") == 0) {
            printf("@ Detected return statement at line %d\n", line_number);
            fprintf(output_file, "    return ");
            for (int i = 1; i < word_count; i++) {
                fprintf(output_file, "%s", words[i]);
                if (i < word_count - 1) {
                    fprintf(output_file, " ");
                }
            }
            fprintf(output_file, ";\n");
            continue;
        }

        if (strcmp(words[0], "print") == 0) {
            printf("@ Detected print statement at line %d\n", line_number);
            fprintf(output_file, "    printf(\"%%f\\n\", ");
            for (int i = 1; i < word_count; i++) {
                fprintf(output_file, "%s", words[i]);
                if (i < word_count - 1) {
                    fprintf(output_file, " ");
                }
            }
            fprintf(output_file, ");\n");
            continue;
        }

        if (word_count >= 3 && strcmp(words[1], "<-") == 0) {
            printf("@ Detected assignment at line %d\n", line_number);
            fprintf(output_file, "    double %s = ", words[0]);
            for (int i = 2; i < word_count; i++) {
                fprintf(output_file, "%s", words[i]);
                if (i < word_count - 1) {
                    fprintf(output_file, " ");
                }
            }
            fprintf(output_file, ";\n");
            continue;
        }
    }

    if (in_function) {
        fprintf(output_file, "}\n");  // Close any unclosed function block
    }

    // End the main function
    fprintf(output_file, "    return 0;\n}\n");

    fclose(input_file);
    fclose(output_file);

    printf("@ Compilation and execution starting...\n");

    char compile_command[256];
    snprintf(compile_command, sizeof(compile_command), "gcc -std=c11 -Wall -Werror -o ml-%d %s", getpid(), c_filename);
    if (system(compile_command) != 0) {
        fprintf(stderr, "! Compilation failed\n");
        return 1;
    }

    char run_command[256];
    snprintf(run_command, sizeof(run_command), "ml-%d", getpid());
    if (system(run_command) != 0) {
        fprintf(stderr, "! Execution failed\n");
        return 1;
    }

    char remove_command[256];
    snprintf(remove_command, sizeof(remove_command), "rm %s ml-%d", c_filename, getpid());
    system(remove_command);

    printf("@ Cleanup completed.\n");

    return 0;
}
