#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <zlib.h>
#include "unzip.h"

#define MAX_PATH_LENGTH 256
char vitasdk_path[MAX_PATH_LENGTH] = {0};

char apk_path[MAX_PATH_LENGTH] = {0};

#define LOG_RESET "\033[0m"
#define LOG_WHITE "\033[97m"
#define LOG_RED "\033[91m"
#define LOG_GREEN "\033[92m"
#define LOG_MAGENTA "\033[95m"

#define APK_PATTERN "*.apk"
#define SO_PATTERN "*.so"

int test_exec(char* program) {
    int test_ret = -1;
    
    char path[MAX_PATH_LENGTH] = {0};
    #ifdef _WIN32
        snprintf(path, sizeof(path), "%s\\arm-vita-eabi\\bin\\readelf.exe", vitasdk_path);
    #else
        snprintf(path, sizeof(path), "%s/arm-vita-eabi/bin/readelf", vitasdk_path);
    #endif

    printf(LOG_MAGENTA "[vapkc] ! Testing %s at path \"%s\"\n" LOG_RESET, program, path);

    char command[MAX_PATH_LENGTH + 40];
    #ifdef _WIN32
        snprintf(command, sizeof(command), "\"%s\" --version > NUL 2>&1", path);
    #else
        // For Linux/Unix: redirect to /dev/null
        snprintf(command, sizeof(command), "\"%s\" --version > /dev/null 2>&1", path);
    #endif

    test_ret = system(command);
    if (test_ret != 0) {
        fprintf(stderr, LOG_RED "[vapkc] !!! Failed to execute %s. Please check the path of your VitaSDK installation.\n" LOG_RESET, program);
        exit(1);
    }
}

int main() {
    FILE* config = fopen("config.txt", "r");
    if (config == NULL) {
        printf(LOG_MAGENTA "[vapkc] ! config.txt not found, commencing setup\n" LOG_RESET);

        printf("[vapkc] Enter the full path to your local VitaSDK installation: ");
        if (fgets(vitasdk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            vitasdk_path[strcspn(vitasdk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "[vapkc] !!! Error reading input\n" LOG_RESET);
            return 1;
        }

        printf(LOG_MAGENTA "[vapkc] ! VitaSDK path set to \"%s\". Testing needed executables...\n" LOG_RESET, vitasdk_path);

        test_exec("readelf");
        test_exec("objdump");

        config = fopen("config.txt", "w");
        if (config == NULL) {
            fprintf(stderr, LOG_RED "[vapkc] !!! Error writing to config.txt\n" LOG_RESET);
            return 1;
        }

        fprintf(config, "vitasdk=%s\n", vitasdk_path);
        fclose(config);

        printf(LOG_GREEN "[vapkc] Paths saved to config.txt\n" LOG_RESET);

    } else {
        char line[MAX_PATH_LENGTH + 20];
        while (fgets(line, sizeof(line), config)) {
            char* equals = strchr(line, '=');
            if (equals) {
                *equals = '\0';
                char* key = line;
                char* value = equals + 1;
                value[strcspn(value, "\n")] = 0;

                if (strcmp(key, "vitasdk") == 0) {
                    strncpy(vitasdk_path, value, MAX_PATH_LENGTH);
                }
            }
        }
        fclose(config);

        printf("%s[vapkc] config.txt found. Loaded path:%s\n", LOG_GREEN, LOG_RESET);
        printf("  - vitasdk: %s\n", vitasdk_path);

        printf("[vapkc] Enter the path to the .APK you want to analyze: ");
        if (fgets(apk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            apk_path[strcspn(apk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "[vapkc] !!! Error reading input\n" LOG_RESET);
            return 1;
        }

        unz_file_info64 apk_file_info;
        unzFile apk_file = unzOpen(apk_path);
        if (!apk_file) {
            fprintf(stderr, LOG_RED "[vapkc] !!! Error opening APK file\n" LOG_RESET);
            return 1;
        }

        printf(LOG_GREEN "\n#########################################\n" LOG_RESET);
            printf(LOG_WHITE "[vapkc] APK ~ \"%s\"\n" LOG_RESET, apk_path);

            int possible_arm7 = 0;
            int possible_arm6 = 0;
            int possible_unity = 0;
                int possible_unity_il2cpp = 0;
            int possible_gdx = 0;
            int possible_gdx_glesv3 = 0;

            int err = 0;

            unzLocateFile(apk_file, "AndroidManifest.xml", NULL);
            unzGetCurrentFileInfo64(apk_file, &apk_file_info, NULL, 0, NULL, 0, NULL, 0);

            unzOpenCurrentFile(apk_file);
                uint64_t manifest_size = apk_file_info.uncompressed_size;

                char *content = (char *)malloc(apk_file_info.uncompressed_size);
                if (content == NULL) {
                    perror("Failed to allocate memory for content");
                    return;
                }

                const size_t read_size = 1 << 15;
                size_t offset = 0;

                while (1) {
                    int len = unzReadCurrentFile(apk_file, content + offset, read_size);
                    if (len == 0) {
                        break;
                    } else if (len < 0) {
                        unzCloseCurrentFile(apk_file);
                        unzClose(apk_file);
                        fprintf(stderr, "failed to read AndroidManifest.xml in APK\n");
                        free(content);
                        return;
                    }
                    offset += len;
                }
            unzCloseCurrentFile(apk_file);

            FILE *output_file = fopen("AndroidManifest.xml", "wb");
            if (output_file == NULL) {
                perror("Failed to open output file");
                free(content);
                return;
            }
        
            size_t written = fwrite(content, 1, apk_file_info.uncompressed_size, output_file);
            if (written != apk_file_info.uncompressed_size) {
                fprintf(stderr, "Failed to write complete content to output file\n");
            }
        
            fclose(output_file);

            free(content);

        printf(LOG_GREEN "#########################################\n\n" LOG_RESET);
        unzClose(apk_file);
    }

    return 0;
}
