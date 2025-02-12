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
#define LOG_YELLOW "\033[93m"
#define LOG_GREY "\033[90m"
#define LOG_CYAN "\033[96m"

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

    printf(LOG_MAGENTA "! Testing %s at path \"%s\"\n" LOG_RESET, program, path);

    char command[MAX_PATH_LENGTH + 40];
    #ifdef _WIN32
        snprintf(command, sizeof(command), "\"%s\" --version > NUL 2>&1", path);
    #else
        // For Linux/Unix: redirect to /dev/null
        snprintf(command, sizeof(command), "\"%s\" --version > /dev/null 2>&1", path);
    #endif

    test_ret = system(command);
    if (test_ret != 0) {
        fprintf(stderr, LOG_RED "!!! Failed to execute %s. Please check the path of your VitaSDK installation.\n" LOG_RESET, program);
        exit(1);
    }
}

int main() {
    printf(LOG_CYAN "vita_apk_check%s ~ v0.1\n", LOG_RESET);
    printf("by hatoving, based on withLogic's Python script\n\n");

    FILE* config = fopen("config.txt", "r");
    if (config == NULL) {
        printf(LOG_MAGENTA "! config.txt not found, commencing setup\n" LOG_RESET);

        printf("Enter the full path to your local VitaSDK installation: ");
        if (fgets(vitasdk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            vitasdk_path[strcspn(vitasdk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "!!! Error reading input\n" LOG_RESET);
            return 1;
        }

        printf(LOG_MAGENTA "! VitaSDK path set to \"%s\". Testing needed executables...\n" LOG_RESET, vitasdk_path);

        test_exec("readelf");
        test_exec("objdump");

        config = fopen("config.txt", "w");
        if (config == NULL) {
            fprintf(stderr, LOG_RED "!!! Error writing to config.txt\n" LOG_RESET);
            return 1;
        }

        fprintf(config, "vitasdk=%s\n", vitasdk_path);
        fclose(config);

        printf(LOG_GREEN "Paths saved to config.txt\n" LOG_RESET);

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

        printf("%sconfig.txt found. Loaded path:%s\n", LOG_GREEN, LOG_RESET);
        printf("  - vitasdk: %s\n", vitasdk_path);

        printf("\nWhat .APK would you want to analyze? -> ");
        if (fgets(apk_path, MAX_PATH_LENGTH, stdin) != NULL) {
            apk_path[strcspn(apk_path, "\n")] = 0;
        } else {
            fprintf(stderr, LOG_RED "!!! Error reading input\n" LOG_RESET);
            return 1;
        }

        unz_file_info64 apk_file_info;
        unzFile apk_file = unzOpen(apk_path);
        if (!apk_file) {
            fprintf(stderr, LOG_RED "!!! Error opening APK file\n" LOG_RESET);
            return 1;
        }

        printf(LOG_GREY "\n#########################################\n" LOG_RESET);
            printf(LOG_WHITE "APK ~ \"%s\"\n" LOG_RESET, apk_path);
            printf(LOG_WHITE "Checking available libraries...\n\n" LOG_RESET);

            int is_arm7 = 0;
            int is_arm6 = 0;
            int is_unity = 0;
                int is_unity_il2cpp = 0;
            int is_gms = 0;
            int is_sdl2 = 0;
                int is_sdl2_mixer = 0;
                int is_sdl2_image = 0;
            int is_java_gdx = 0;
            int is_gles3 = 0;
            int has_fmod = 0;
                int has_fmodstudio = 0;
                int has_fmodex = 0;
            int is_cocos2d = 0;

            int err = 0;

            printf(LOG_WHITE "Library colors rank from portable\nto unknown (%sPortable%s, %sFeasible%s,\n%sTheoretically Possible%s, %sNot\nPortable%s, %sUnknown%s)\n\n" LOG_RESET,
                LOG_GREEN, LOG_RESET, LOG_YELLOW, LOG_RESET, LOG_MAGENTA, LOG_RESET, LOG_RED, LOG_RESET, LOG_GREY, LOG_RESET);

            err = unzGoToFirstFile(apk_file);
            if (err != UNZ_OK) {
                fprintf(stderr, LOG_RED "!!! Error going to first file in APK\n" LOG_RESET);
                return 1;
            }

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
                        if (strstr(filename, "libunity.so") != NULL) {
                            is_unity = 1;
                            printf(LOG_MAGENTA "!! Found Unity library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else if (strstr(filename, "libil2cpp.so") != NULL) {
                            is_unity_il2cpp = 1;
                            printf(LOG_MAGENTA "!! Found Unity library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else if (strstr(filename, "libmono.so") != NULL) {
                            printf(LOG_MAGENTA "!! Found Mono library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else if (strstr(filename, "libgdx.so") != NULL) {
                            is_java_gdx = 1;
                            printf(LOG_MAGENTA "!! Found libgdx library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else if (strstr(filename, "libyoyo.so") != NULL) {
                            is_gms = 1;
                            printf(LOG_MAGENTA "!! Found GameMaker library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else if (strstr(filename, "libSDL2") != NULL) {
                            is_sdl2 = 1;
                            if (strstr(filename, "libSDL2_image") != NULL) {
                                is_sdl2_image = 1;
                            }
                            if (strstr(filename, "libSDL2_mixer") != NULL) {
                                is_sdl2_mixer = 1;
                            }
                            printf(LOG_GREEN "!! Found SDL2 library file:\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        } else {
                            printf(LOG_GREY "!! Found library file:\t\t%s\t(%.2f mB)\n" LOG_RESET, filename, (double)file_info.uncompressed_size / (1024 * 1024));
                        }
                    }
                }

                err = unzGoToNextFile(apk_file);
            } while (err == UNZ_OK);

            if (err != UNZ_END_OF_LIST_OF_FILE) {
                fprintf(stderr, LOG_RED "!!! Error iterating through files\n" LOG_RESET);
                return 1;
            }

        printf(LOG_WHITE "\n~~ ANALYSIS CONCLUSION ~~\n" LOG_RESET);

        if (is_arm7) {
            printf(LOG_GREEN "- The .APK has ARMv7 libraries.\n" LOG_RESET);
        }
        if (is_arm6) {
            printf(LOG_GREEN "- The .APK has ARMv6 libraries.\n" LOG_RESET);
        }
        if (is_gms) {
            printf(LOG_GREEN "- The .APK is made in either made in GameMaker\n  or GameMaker: Studio. You should be able to use\n  YoYoLoader by Rinnegatamante for\n  this game.\n" LOG_RESET);
        }
        if (is_unity) {
            printf(LOG_MAGENTA "- The .APK is made in Unity, which is going to\n  severely hinder the possiblity of it being\n  portable." LOG_RESET);
            if (is_unity_il2cpp) {
                printf(LOG_RED " Since it's compiled under the\n  IL2CPP scripting backend, you are going\n  to be better off not trying not to port\n  this.\n" LOG_RESET);
            } else {
                printf(LOG_MAGENTA " However, since it's compiled\n  under the Mono scripting backend, it may be\n  possible to decompile and port it to\n  the PlayStation Vita that way.\n" LOG_RESET);
            }
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
        if (!is_arm7 && !is_arm6) {
            printf(LOG_RED "No compatible ARM libraries were found.\n\tThe .APK is most likely not able to be\n\tported to the PlayStation Vita.\n" LOG_RESET);
        }
        printf(LOG_WHITE "~~ ANALYSIS CONCLUSION ~~\n" LOG_RESET);

        printf(LOG_GREY "#########################################\n\n" LOG_RESET);
        unzClose(apk_file);
    }

    return 0;
}
