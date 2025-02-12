#pragma region Libraries

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <zlib.h>
#include "unzip.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#pragma endregion

#pragma region Constants

#define MAX_PATH_LENGTH 256
#define MAX_LINE_LENGTH 512
#define OUTPUT_BUFFER_SIZE 4096
#define MAX_LIBS 50

char vitasdk_path[MAX_PATH_LENGTH] = {0};
char apk_path[MAX_PATH_LENGTH] = {0};

typedef struct {
    char filename[MAX_PATH_LENGTH];
    void *data;          // Pointer to the loaded file data
    size_t size;         // Size of the file in bytes
} so_file;

#define LOG_RESET "\033[0m"
#define LOG_WHITE "\033[97m"
#define LOG_RED "\033[91m"
#define LOG_GREEN "\033[92m"
#define LOG_MAGENTA "\033[95m"
#define LOG_YELLOW "\033[93m"
#define LOG_GREY "\033[90m"
#define LOG_CYAN "\033[96m"

#ifdef _WIN32
    #include <direct.h>
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/stat.h>
    #define MKDIR(path) mkdir(path, 0777)
#endif

#pragma endregion

#pragma region Functions
char* my_strcasestr(const char* haystack, const char* needle) {
    if (!needle[0]) return (char*) haystack;

    for (size_t i = 0; haystack[i]; i++) {
        int matches = 1;
        for (size_t j = 0; needle[j]; j++) {
            if (!haystack[i + j]) return NULL;

            if (tolower((unsigned char)needle[j]) != 
                tolower((unsigned char)haystack[i + j])) {
                matches = 0;
                break;
            }
        }
        if (matches) return (char *)(haystack + i);
    }
    return NULL;
}

int run_exec(char* program, char* args) {
    char path[MAX_PATH_LENGTH] = {0};
    #ifdef _WIN32
        snprintf(path, sizeof(path), "%s\\arm-vita-eabi\\bin", vitasdk_path);
    #else
        snprintf(path, sizeof(path), "%s/arm-vita-eabi/bin", vitasdk_path);
    #endif

    char command[MAX_PATH_LENGTH + 40];
    #ifdef _WIN32
        snprintf(command, sizeof(command), "%s\\%s.exe %s", path, program, args);
    #else
        // For Linux/Unix: redirect to /dev/null
        snprintf(command, sizeof(command), "%s/%s %s", path, program, args);
    #endif

    int test_ret = system(command);
    if (test_ret != 0) {
        fprintf(stderr, LOG_RED "!!! Failed to execute readelf. Please check the path of your VitaSDK installation.\nCommand: \"%s\"\n" LOG_RESET, command);
        return 1;
    }
    return 0;
}

int run_exec_output(char* program, char* args, char** output_buffer, size_t* buffer_size) {
    char path[MAX_PATH_LENGTH] = {0};
    
    // Construct the path to the executable
    #ifdef _WIN32
        snprintf(path, sizeof(path), "%s\\arm-vita-eabi\\bin", vitasdk_path);
    #else
        snprintf(path, sizeof(path), "%s/arm-vita-eabi/bin", vitasdk_path);
    #endif

    // Build the command string
    char command[MAX_PATH_LENGTH + 40];
    #ifdef _WIN32
        snprintf(command, sizeof(command), "%s\\%s.exe %s", path, program, args);
    #else
        snprintf(command, sizeof(command), "%s/%s %s", path, program, args);
    #endif

    // Open a pipe to execute the command and read the output
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, LOG_RED "!!! Failed to execute %s. Please check the path of your VitaSDK installation.\nCommand: \"%s\"\n" LOG_RESET, program, command);
        return 1;
    }

    // Initial buffer allocation
    *buffer_size = 1024;  // Start with 1 KB
    *output_buffer = (char*)malloc(*buffer_size);
    if (*output_buffer == NULL) {
        fprintf(stderr, LOG_RED "!!! Memory allocation failed.\n" LOG_RESET);
        pclose(fp);
        return 1;
    }

    size_t total_read = 0;
    size_t chunk_size = 512;  // Read in 512-byte chunks
    char temp_buffer[chunk_size];

    while (fgets(temp_buffer, chunk_size, fp) != NULL) {
        size_t len = strlen(temp_buffer);

        // Check if we need to reallocate more memory
        if (total_read + len + 1 >= *buffer_size) {
            *buffer_size *= 2;  // Double the buffer size
            char* temp = realloc(*output_buffer, *buffer_size);
            if (temp == NULL) {
                fprintf(stderr, LOG_RED "!!! Memory reallocation failed.\n" LOG_RESET);
                free(*output_buffer);
                pclose(fp);
                return 1;
            }
            *output_buffer = temp;
        }

        // Append new data to the output buffer
        memcpy(*output_buffer + total_read, temp_buffer, len);
        total_read += len;
    }

    // Null-terminate the final output
    (*output_buffer)[total_read] = '\0';

    int ret_code = pclose(fp);
    return ret_code;  // Return the process exit code
}

int test_exec(char* program) {
    #ifdef _WIN32
        return run_exec(program, "--version > NUL 2>&1");
    #else
        // For Linux/Unix: redirect to /dev/null
        return run_exec(program, "--version > /dev/null 2>&1");
    #endif
}

void delete_files_in_directory(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        perror("Unable to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[512];

        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        struct stat statbuf;
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                // Recursively delete contents if it's a directory
                delete_files_in_directory(full_path);
                if (rmdir(full_path) != 0) {
                    perror("Error removing subdirectory");
                }
            } else {
                // Remove file
                if (remove(full_path) != 0) {
                    perror("Error deleting file");
                }
            }
        }
    }

    closedir(dir);
}

#pragma endregion


int main(int argc, char *argv[]){
    printf(LOG_CYAN "vita_apk_check%s ~ v0.1\n", LOG_RESET);
    printf("by hatoving, based on withLogic's Python script\n\n");

    if (argc < 2) {
        fprintf(stderr, LOG_MAGENTA "Usage: %s <apk_path>\n\n" LOG_RESET, argv[0]);
    }
    
    START:

    FILE* config = fopen("config.txt", "r");
    if (config == NULL) {
        #pragma region Start-up
        printf(LOG_MAGENTA "! config.txt not found, commencing setup\n" LOG_RESET);

        printf("Enter the full path to your local VitaSDK installation: ");
        if (fgets(vitasdk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            vitasdk_path[strcspn(vitasdk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "!!! Error reading input\n" LOG_RESET);
            return 1;
        }

        printf(LOG_MAGENTA "! VitaSDK path set to \"%s\". Testing needed executables...\n" LOG_RESET, vitasdk_path);

        if (test_exec("readelf") || test_exec("objdump")) {
            fprintf(stderr, LOG_RED "!!! Error testing executables\n" LOG_RESET);
            return 1;
        }

        config = fopen("config.txt", "w");
        if (config == NULL) {
            fprintf(stderr, LOG_RED "!!! Error writing to config.txt\n" LOG_RESET);
            return 1;
        }

        fprintf(config, "vitasdk=%s\n", vitasdk_path);
        fclose(config);

        printf(LOG_GREEN "Paths saved to config.txt\n\n" LOG_RESET);
        goto START;
        #pragma endregion

    } else {
        #pragma region Load APK File
        
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

        printf("%sconfig.txt found. Loaded path:%s\n", LOG_GREEN, LOG_RESET);
        printf("  - vitasdk: %s\n", vitasdk_path);

        if (argc > 1) {
            strncpy(apk_path, argv[1], sizeof(apk_path) - 1);
            goto LOAD_APK;
        }

        printf("\nWhat .APK would you want to analyze? -> ");
        if (fgets(apk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            apk_path[strcspn(apk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "!!! Error reading input\n" LOG_RESET);
            return 1;
        }

        LOAD_APK:

        unz_file_info64 apk_file_info;
        unzFile apk_file = unzOpen(apk_path);
        if (!apk_file) {
            fprintf(stderr, LOG_RED "!!! Error opening APK file\n" LOG_RESET);
            return 1;
        }

        #pragma endregion
        #pragma region Analysis Part 1

        printf(LOG_GREY "\n#########################################\n" LOG_RESET);
            printf(LOG_WHITE "APK ~ \"%s\"\n" LOG_RESET, apk_path);
            printf(LOG_WHITE "Checking available libraries...\n\n" LOG_RESET);

            int is_arm7 = 0;
            int is_arm6 = 0;
            int is_unity = 0;
                int is_unity_il2cpp = 0;
                int is_mono = 0;

            int is_gms = 0;

            int is_sdl2 = 0;
                int is_sdl2_mixer = 0;
                int is_sdl2_image = 0;

            int is_java_gdx = 0;
            int has_gles3 = 0;

            int has_fmod = 0;
                int has_fmodstudio = 0;
                int has_fmodex = 0;
                int fmod_sym_count = 0;

            int is_cocos2d = 0;
                int cocos2d_sym_count = 0;

            int is_clickteam = 0;
                int is_clickteam_chowdren = 0;
                
            int is_unreal = 0;
                int is_unreal4 = 0;

            int has_java_sym = 0;
                int java_sym_count = 0;
                int has_jni_sym = 0;
            int has_cpp_sym = 0;
            int has_lua_sym = 0;

            int has_opensles = 0;
                int opensles_sym_count = 0;

            int is_native_android = 0;
                int native_android_sym_count = 0;

            int err = 0;

            printf(LOG_WHITE "Library colors rank from portable to unknown\n(%sPortable%s, %sFeasible%s, %sIn Theory%s, %sNot Portable%s, %sUnknown%s)\n\n" LOG_RESET,
                LOG_GREEN, LOG_RESET, LOG_YELLOW, LOG_RESET, LOG_MAGENTA, LOG_RESET, LOG_RED, LOG_RESET, LOG_GREY, LOG_RESET);
                
            err = unzGoToFirstFile(apk_file);
            if (err != UNZ_OK) {
                fprintf(stderr, LOG_RED "!!! Error going to first file in APK\n" LOG_RESET);
                return 1;
            }

            so_file loaded_libs[MAX_LIBS] = {0};
            int lib_count = 0;
            
            MKDIR("temp");
            MKDIR("temp\\lib");
            MKDIR("temp\\lib\\armeabi");
            MKDIR("temp\\lib\\armeabi-v7a");

            do {
                char filename[MAX_PATH_LENGTH];
                unz_file_info file_info;
                err = unzGetCurrentFileInfo(apk_file, &file_info, filename, MAX_PATH_LENGTH, NULL, 0, NULL, 0);
                if (err != UNZ_OK) {
                    fprintf(stderr, LOG_RED "!!! Error getting file info\n" LOG_RESET);
                    return 1;
                }

                if (strstr(filename, "lib/armeabi-v7a/") != NULL) {
                    is_arm7 = 1;
                } else if (strstr(filename, "lib/armeabi/") != NULL) {
                    is_arm6 = 1;
                } else {
                    err = unzGoToNextFile(apk_file);
                    continue;
                }

                if (is_arm7 || is_arm6) {
                    if (strstr(filename, ".so") != NULL) {
                        double file_size_mb = (double)file_info.uncompressed_size / (1024 * 1024);
                
                        if (strstr(filename, "libunity.so") != NULL) {
                            is_unity = 1;
                            printf(LOG_MAGENTA "!! Found Unity library file:          %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libil2cpp.so") != NULL) {
                            is_unity_il2cpp = 1;
                            printf(LOG_RED "!! Found Unity IL2CPP library file:   %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libmono.so") != NULL) {
                            is_mono = 1;
                            printf(LOG_MAGENTA "!! Found Mono library file:           %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libgdx.so") != NULL) {
                            is_java_gdx = 1;
                            printf(LOG_MAGENTA "!! Found libGDX library file:         %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libyoyo.so") != NULL) {
                            is_gms = 1;
                            printf(LOG_MAGENTA "!! Found GameMaker library file:      %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libfmod") != NULL) {
                            has_fmod = 1;
                            if (strstr(filename, "libfmodstudio") != NULL) {
                                has_fmodstudio = 1;
                            } else if (strstr(filename, "libfmodex") != NULL) {
                                has_fmodex = 1;
                            }
                            printf(LOG_MAGENTA "!! Found FMOD library file:           %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else if (strstr(filename, "libSDL2") != NULL) {
                            is_sdl2 = 1;
                            if (strstr(filename, "libSDL2_image") != NULL) {
                                is_sdl2_image = 1;
                            }
                            if (strstr(filename, "libSDL2_mixer") != NULL) {
                                is_sdl2_mixer = 1;
                            }
                            printf(LOG_GREEN "!! Found SDL2 library file:           %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        } else {
                            printf(LOG_GREY "!! Found library file:                %-50s (%8.2f MB)\n" LOG_RESET, filename, file_size_mb);
                        }

                        void *buffer = malloc(file_info.uncompressed_size);
                        if (!buffer) {
                            fprintf(stderr, "Error: Unable to allocate memory for %s\n", filename);
                            err = unzGoToNextFile(apk_file);
                            continue;
                        }

                        // Open and read the file into memory
                        err = unzOpenCurrentFile(apk_file);
                        if (err != UNZ_OK) {
                            fprintf(stderr, "Error opening file: %s\n", filename);
                            free(buffer);
                            err = unzGoToNextFile(apk_file);
                            continue;
                        }

                        int bytes_read = unzReadCurrentFile(apk_file, buffer, file_info.uncompressed_size);
                        if (bytes_read < 0) {
                            fprintf(stderr, "Error reading file: %s\n", filename);
                            free(buffer);
                            unzCloseCurrentFile(apk_file);
                            err = unzGoToNextFile(apk_file);
                            continue;
                        }

                        unzCloseCurrentFile(apk_file);

                        // Store the loaded file in our array
                        strcpy(loaded_libs[lib_count].filename, filename);
                        loaded_libs[lib_count].data = buffer;
                        loaded_libs[lib_count].size = file_info.uncompressed_size;

                        char temp_file_path[MAX_PATH_LENGTH];
                        snprintf(temp_file_path, sizeof(temp_file_path), "temp/%s", filename);

                        FILE *out_file = fopen(temp_file_path, "wb");
                        fwrite(buffer, 1, file_info.uncompressed_size, out_file);
                        fclose(out_file);
                        
                        lib_count++;
                    }
                }                

                err = unzGoToNextFile(apk_file);
            } while (err == UNZ_OK);

            if (err != UNZ_END_OF_LIST_OF_FILE) {
                fprintf(stderr, LOG_RED "!!! Error iterating through files\n" LOG_RESET);
                return 1;
            }
        #pragma endregion        
        #pragma region Analysis Part 2
        fprintf(stdout, LOG_WHITE "\nAnalyzing .SO files...\n" LOG_RESET);
        for (int i = 0; i < lib_count; i++) {
            char* output_buffer = NULL;
            size_t buffer_size = 0;
            char command[MAX_PATH_LENGTH + 40];

            snprintf(command, sizeof(command), "-T -C temp/%s", loaded_libs[i].filename);

            if (!run_exec_output("objdump", command, &output_buffer, &buffer_size)) {
                FILE *out_file = fopen("symbols.txt", "wb");
                fwrite(output_buffer, 1, buffer_size, out_file);
                fclose(out_file);

                out_file = fopen("symbols.txt", "r");

                char line[MAX_LINE_LENGTH];
                while (fgets(line, sizeof(line), out_file)) {
                    // Remove trailing newline
                    line[strcspn(line, "\n")] = '\0';

                    if (strstr(line, "Java_") != NULL) {         
                        has_java_sym = 1;
                        java_sym_count++;
                    }
                    if (strstr(line, "Java_org_cocos2d") != NULL) {         
                        is_cocos2d = 1;
                        cocos2d_sym_count++;
                    }
                    if (strstr(line, "slCreateEngine") || strstr(line, "SL_IID")) {
                        has_opensles = 1;
                        opensles_sym_count++;
                    }
                    if (strstr(line, "android_main") || strstr(line, "ANativeActivity_")
                        || strstr(line, "AAssetManager_") || strstr(line, "AInputQueue_")
                        || strstr(line, "AMotionEvent_") || strstr(line, "ASensor_")
                        || strstr(line, "ALooper_") || strstr(line, "AConfiguration_")
                        || strstr(line, "AInputQueue_") || strstr(line, "AAssetDir_")
                        || strstr(line, "AAsset_") || strstr(line, "AInputEvent_")
                        || strstr(line, "AKeyEvent")) {
                        is_native_android = 1;
                        native_android_sym_count++;
                    }
                    if (my_strcasestr(line, "FMOD_") != NULL || my_strcasestr(line, "_ZN4FMOD14") != NULL) {         
                        has_fmod = 1;
                        fmod_sym_count++;
                    }
                }

                if (has_java_sym) {
                    printf(LOG_YELLOW "!! Found  %i\tJava_* symbol(s) in      \"%s\"\n" LOG_RESET, java_sym_count, loaded_libs[i].filename);
                }
                if (has_opensles) {
                    printf(LOG_MAGENTA "!! Found  %i\tOpenSLES symbol(s) in    \"%s\"\n" LOG_RESET, opensles_sym_count, loaded_libs[i].filename);
                }
                if (has_fmod) {
                    printf(LOG_MAGENTA "!! Found  %i\tFMOD symbol(s) in        \"%s\"\n" LOG_RESET, fmod_sym_count, loaded_libs[i].filename);
                }
                if (is_cocos2d) {
                    printf(LOG_YELLOW "!! Found  %i\tCocos2D symbol(s) in     \"%s\"\n" LOG_RESET, cocos2d_sym_count, loaded_libs[i].filename);
                }
                if (is_native_android) {
                    printf(LOG_YELLOW "!! Found  %i\tNDK symbol(s) in         \"%s\"\n" LOG_RESET, native_android_sym_count, loaded_libs[i].filename);
                }

                fclose(out_file);

            } else {
                printf(LOG_RED "!!! Failed to capture output.\n" LOG_RESET);
            }
            free(output_buffer);
        }

        delete_files_in_directory("temp/");
        if (rmdir("temp/") != 0) {
            perror("Error deleting temporary directory");
        }
        printf(LOG_WHITE "\nAll of the symbols have been saved in\nthe \"symbols.txt\" file in the same folder\nyou're running the executable in.\n" LOG_RESET);

        #pragma endregion

        #pragma region Analysis Conclusion

        printf(LOG_WHITE "\n~~ ANALYSIS CONCLUSION ~~\n" LOG_RESET);

        if (is_arm7) {
            printf(LOG_GREEN "- The .APK has ARMv7 libraries.\n" LOG_RESET);
        }
        if (is_arm6) {
            printf(LOG_GREEN "- The .APK has ARMv6 libraries.\n" LOG_RESET);
        }
        if (is_native_android && !is_cocos2d) {
            printf(LOG_YELLOW "- The .APK uses the Android NDK, which means that\n  it very likely has android_main as it's main\n  entry point and that it uses certain NDK functions\n  like ANativeActivity_onCreate. You should check\n  out v-atamanenko's port of Galaxy on Fire 2 for\n  reference and use FalsoNDK, also by v-atamanenko,\n  to port the .APK.\n" LOG_RESET);
        }
        if (is_gms) {
            printf(LOG_GREEN "- The .APK is made in either made in GameMaker\n  or GameMaker: Studio. You should be able to use\n  YoYoLoader by Rinnegatamante for\n  this game.\n" LOG_RESET);
        }
        if (is_java_gdx) {
            printf(LOG_MAGENTA "- The .APK uses libGDX as it's framework, which\n  likely means the entire engine/game is written\n  in Java. This can theoretically ported if everything\n  Java related gets reimplemented on the\n  loader side." LOG_RESET);
        }
        if (is_cocos2d) {
            printf(LOG_YELLOW "- The .APK uses Cocos2dx as it's framework, which\n  means that the .APK is very much possible to be\n  ported. You should be aware however that the actual\n  logic of how it handles certain systems (e.g. text\n  rendering) could very much hinder the\n  possiblity of it being ported.\n" LOG_RESET);
        }
        if (is_mono) {
            printf(LOG_MAGENTA "- The .APK uses Mono, which means the game likely\n  uses .NET libraries/C# code to execute it's\n  logic.\n" LOG_RESET);
        }
        if (is_unity) {
            printf(LOG_MAGENTA "- The .APK is made in Unity, which is going to\n  severely hinder the possiblity of it being\n  portable." LOG_RESET);
            if (is_unity_il2cpp) {
                printf(LOG_RED " Since it's compiled under the\n  IL2CPP scripting backend, you are going\n  to be better off not trying not to port\n  this.\n" LOG_RESET);
            } else {
                printf(LOG_MAGENTA " However, since it's compiled\n  under the Mono scripting backend, it may be\n  possible to decompile and port it to\n  the PlayStation Vita that way.\n" LOG_RESET);
            }
        }
        if (has_fmod) {
            printf(LOG_MAGENTA "- The .APK uses FMOD, which likely means that it's\n  using it as it's audio backend. You are not going\n  to be able to port this on your own, except if you\n  legally own the libraries and files needed to build\n  the loader with FMOD." LOG_RESET);
            if (has_fmodstudio) {
                printf(LOG_MAGENTA " It uses FMOD Studio too, which\n  is able to be ported." LOG_RESET);
            }
            if (has_fmodex) {
                printf(LOG_RED " However, it also uses FMOD Ex,\n  making it unable to be ported legally." LOG_RESET);
            }
            printf("\n");
        }
        if (is_sdl2) {
            printf(LOG_GREEN "- The .APK uses SDL2, meaning it also likely has\n  SDL_main as it's main entry point, making\n  it easy to bootstrap.\n" LOG_RESET);

            if (is_sdl2_image || is_sdl2_mixer) {
                printf(LOG_GREEN "  -  Additionally found libraries are:\n" LOG_RESET);
                if (is_sdl2_image) {
                    printf(LOG_GREEN "    ! SDL2_image\n" LOG_RESET);
                }
                if (is_sdl2_mixer) {
                    printf(LOG_GREEN "    ! SDL2_mixer\n" LOG_RESET);
                }
            }
        }
        if (has_opensles && !has_fmod) {
            printf(LOG_MAGENTA "- The .APK uses OpenSLES. Although VitaSDK has an\n  implementation for OpenSLES, including for the\n  specific Android functions, it can be very finicky\n  to work with.\n" LOG_RESET);
        }
        if (!is_arm7 && !is_arm6) {
            printf(LOG_RED "- No compatible ARM libraries were found.\n  The .APK is not able to be ported to then\n  PlayStation Vita.\n" LOG_RESET);
        }
        printf(LOG_WHITE "~~ ANALYSIS CONCLUSION ~~\n" LOG_RESET);

        printf(LOG_GREY "#########################################\n\n" LOG_RESET);
        unzClose(apk_file);

        for (int i = 0; i < lib_count; i++) {
            free(loaded_libs[i].data);
        }

        #pragma endregion
    }

    return 0;
}