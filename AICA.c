#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Structure to hold lines of code
typedef struct {
    char *line;
    int line_number;
    int matched; // Field to track if this line has been matched
} CodeLine;

// Function to trim whitespace from a line
char *trim_whitespace(char *str) {
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    // All spaces?
    if (*str == 0) return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end + 1) = '\0';

    return str;
}

// Function to remove comments from a line
char *remove_comments(char *line, int *in_block_comment) {
    char *result = malloc(strlen(line) + 1);
    if (!result) {
        fprintf(stderr, "Memory allocation error.\n");
        exit(1);
    }
    int i = 0, j = 0;
    int len = strlen(line);

    while (i < len) {
        if (*in_block_comment) {
            if (line[i] == '*' && i + 1 < len && line[i + 1] == '/') {
                *in_block_comment = 0;
                i += 2;
            } else {
                i++;
            }
        } else {
            if (line[i] == '/' && i + 1 < len) {
                if (line[i + 1] == '/') {
                    // Line comment detected; ignore the rest of the line
                    break;
                } else if (line[i + 1] == '*') {
                    // Start of block comment
                    *in_block_comment = 1;
                    i += 2;
                    continue;
                }
            }
            // Copy non-comment characters
            result[j++] = line[i++];
        }
    }
    result[j] = '\0';
    return result;
}

// Function to get the Desktop path dynamically
const char* get_desktop_path(char *buffer, size_t size) {
    const char *user_profile = getenv("USERPROFILE");
    if (user_profile == NULL) {
        fprintf(stderr, "Error: USERPROFILE environment variable not found.\n");
        exit(1);
    }

    // Construct the Desktop path
    snprintf(buffer, size, "%s\\Desktop\\output.rtf", user_profile);
    return buffer;
}

// Function to get the path for input files dynamically
void get_input_file_path(const char *file_name, char *buffer, size_t size) {
    const char *user_profile = getenv("USERPROFILE");
    if (user_profile == NULL) {
        fprintf(stderr, "Error: USERPROFILE environment variable not found.\n");
        exit(1);
    }

    // Construct the Desktop path for input files
    snprintf(buffer, size, "%s\\Desktop\\%s", user_profile, file_name);
}

// Function to read code from a file while removing comments
int read_code(const char *filename, CodeLine **code_lines) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file: %s\n", filename);
        return -1;
    }

    char buffer[1024];
    int line_count = 0;
    int allocated_size = 100; // Start with space for 100 lines, will grow as needed
    int in_block_comment = 0;

    // Allocate initial memory
    *code_lines = (CodeLine *)malloc(allocated_size * sizeof(CodeLine));
    if (!*code_lines) {
        fprintf(stderr, "Memory allocation error.\n");
        fclose(file);
        return -1;
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        // Remove comments from the line
        char *code = remove_comments(buffer, &in_block_comment);
        // Trim whitespace
        char *trimmed_code = trim_whitespace(code);

        // Skip empty lines
        if (strlen(trimmed_code) == 0) {
            free(code);
            continue;
        }

        if (line_count >= allocated_size) {
            // Increase the allocated memory size as needed
            allocated_size *= 2;
            *code_lines = (CodeLine *)realloc(*code_lines, allocated_size * sizeof(CodeLine));
            if (!*code_lines) {
                fprintf(stderr, "Memory allocation error.\n");
                fclose(file);
                return -1;
            }
        }

        (*code_lines)[line_count].line = strdup(trimmed_code);
        if (!(*code_lines)[line_count].line) {
            fprintf(stderr, "Memory allocation error.\n");
            free(code);
            fclose(file);
            return -1;
        }
        (*code_lines)[line_count].line_number = line_count + 1;
        (*code_lines)[line_count].matched = 0; // Initialize matched field to 0
        line_count++;

        free(code);
    }

    fclose(file);
    return line_count;
}

// Function to check if a line exists and hasn't been matched
int find_matching_line(const char *line, CodeLine *array, int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(line, array[i].line) == 0 && array[i].matched == 0) {
            array[i].matched = 1; // Mark this line as matched
            return 1; // Line exists and was unmatched
        }
    }
    return 0; // Line does not exist or is already matched
}

// Function to escape RTF special characters
void escape_rtf(const char *input, FILE *rtf_file) {
    while (*input) {
        if (*input == '\\' || *input == '{' || *input == '}') {
            fprintf(rtf_file, "\\%c", *input);
        } else if (*input == '\n') {
            fprintf(rtf_file, "\\line ");
        } else {
            fputc(*input, rtf_file);
        }
        input++;
    }
}

// Function to compare lines and write the same/different ones to the RTF file
void compare_lines(CodeLine *original, int original_count, CodeLine *ai_generated, int ai_count, FILE *rtf_file) {
    // Display lines that are the same
    fprintf(rtf_file, "\\b\\cf1 Lines that are the same:\\b0\\cf0\\line ");
    int same_line_found = 0;
    for (int i = 0; i < original_count; i++) {
        if (find_matching_line(original[i].line, ai_generated, ai_count)) {
            fprintf(rtf_file, "\\cf1 Same line %d: ", original[i].line_number);
            escape_rtf(original[i].line, rtf_file);
            fprintf(rtf_file, "\\line ");
            same_line_found = 1;
        }
    }
    if (!same_line_found) {
        fprintf(rtf_file, "No lines are the same.\\line ");
    }

    // Reset matched flags for rechecking
    for (int i = 0; i < original_count; i++) {
        original[i].matched = 0;
    }
    for (int i = 0; i < ai_count; i++) {
        ai_generated[i].matched = 0;
    }

    // Display lines that are different or unique
    fprintf(rtf_file, "\\b\\cf2 Lines that are different or unique:\\b0\\cf0\\line ");
    int different_line_found = 0;
    for (int i = 0; i < original_count; i++) {
        if (!find_matching_line(original[i].line, ai_generated, ai_count)) {
            fprintf(rtf_file, "\\cf2 Original unique or altered line %d: ", original[i].line_number);
            escape_rtf(original[i].line, rtf_file);
            fprintf(rtf_file, "\\line ");
            different_line_found = 1;
        }
    }
    for (int i = 0; i < ai_count; i++) {
        if (!find_matching_line(ai_generated[i].line, original, original_count)) {
            fprintf(rtf_file, "\\cf2 AI-generated unique or altered line %d: ", ai_generated[i].line_number);
            escape_rtf(ai_generated[i].line, rtf_file);
            fprintf(rtf_file, "\\line ");
            different_line_found = 1;
        }
    }
    if (!different_line_found) {
        fprintf(rtf_file, "No lines are different.\\line ");
    }
}

int main() {
    CodeLine *original_code = NULL;
    CodeLine *ai_generated_code = NULL;

    // Buffer to hold the Desktop path
    char output_path[512];
    get_desktop_path(output_path, sizeof(output_path));

    // Debug: Print the output path
    printf("Output RTF Path: %s\n", output_path);

    // Paths to input files
    char original_input[512];
    char ai_generated_input[512];
    get_input_file_path("original_code.txt", original_input, sizeof(original_input));
    get_input_file_path("ai_generated_code.txt", ai_generated_input, sizeof(ai_generated_input));

    // Debug: Print input file paths
    printf("Original Code Path: %s\n", original_input);
    printf("AI-generated Code Path: %s\n", ai_generated_input);

    // Read original code
    int original_count = read_code(original_input, &original_code);
    if (original_count == -1) {
        // Free any allocated memory before exiting
        if (original_code) {
            for (int i = 0; i < original_count; i++) {
                free(original_code[i].line);
            }
            free(original_code);
        }
        return 1;
    }

    // Read AI-generated code
    int ai_count = read_code(ai_generated_input, &ai_generated_code);
    if (ai_count == -1) {
        // Free any allocated memory before exiting
        if (original_code) {
            for (int i = 0; i < original_count; i++) {
                free(original_code[i].line);
            }
            free(original_code);
        }
        if (ai_generated_code) {
            for (int i = 0; i < ai_count; i++) {
                free(ai_generated_code[i].line);
            }
            free(ai_generated_code);
        }
        return 1;
    }

    // Open the RTF output file
    FILE *rtf_file = fopen(output_path, "w");
    if (rtf_file == NULL) {
        printf("Error creating output file: %s\n", output_path);
        // Free allocated memory before exiting
        for (int i = 0; i < original_count; i++) {
            free(original_code[i].line);
        }
        for (int i = 0; i < ai_count; i++) {
            free(ai_generated_code[i].line);
        }
        free(original_code);
        free(ai_generated_code);
        return 1;
    }

    // Write RTF header
    fprintf(rtf_file, "{\\rtf1\\ansi\n");

    // Define RTF colors: 0 - Black (default), 1 - Green (same), 2 - Red (different)
    // Color table should be defined before any content
    fprintf(rtf_file, "{\\colortbl ;\\red0\\green128\\blue0;\\red255\\green0\\blue0;}\n");

    // Start the document with default formatting
    fprintf(rtf_file, "\\pard\\f0\\fs20\\line\\line ");

    // Compare lines and write to RTF
    compare_lines(original_code, original_count, ai_generated_code, ai_count, rtf_file);

    // Close the RTF document
    fprintf(rtf_file, "}\n");
    fclose(rtf_file);

    printf("Comparison completed. Output saved to %s\n", output_path);

    // Clean up allocated memory
    for (int i = 0; i < original_count; i++) {
        free(original_code[i].line);
    }
    for (int i = 0; i < ai_count; i++) {
        free(ai_generated_code[i].line);
    }
    free(original_code);
    free(ai_generated_code);

    return 0;
}
