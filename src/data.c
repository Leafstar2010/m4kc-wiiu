#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "data.h"
#include "textures.h"

data_WorldListItem *data_worldList = NULL;
int data_worldListLength = 0;

char directoryName            [PATH_MAX] = { 0 };
char optionsFileName          [PATH_MAX] = { 0 };
char worldsDirectoryName      [PATH_MAX] = { 0 };
char screenshotsDirectoryName [PATH_MAX] = { 0 };

static uint32_t getSurfacePixel   (SDL_Surface *, int, int);

int data_init (void) {
        strncpy(directoryName, "fs:/vol/external01/wiiu/m4kcu", PATH_MAX);
        snprintf(optionsFileName, PATH_MAX, "%s/m4kc.conf", directoryName);
        snprintf(worldsDirectoryName, PATH_MAX, "%s/worlds", directoryName);
        snprintf(screenshotsDirectoryName, PATH_MAX, "%s/screenshots", directoryName);
        
        // Try to create the base directory - ignore if it fails
        mkdir(directoryName, 0755);
        
        return 0;
}

int data_directoryExists (const char *path) {
        struct stat directoryInfo;
        return (stat(path, &directoryInfo) == 0 && S_ISDIR(directoryInfo.st_mode));
}

int data_fileExists (const char *path) {
        return access(path, F_OK) == 0;
}

int data_ensureDirectoryExists (const char *path) {
        if (data_directoryExists(path)) return 0;
        mkdir(path, 0755);
        return 0;
}

int data_removeDirectory (const char *path) {
        DIR *directory = opendir(path);
        size_t pathLength = strlen(path);
        if (!directory) return 1;
        struct dirent *directoryEntry;
        int err = 0;
        while (!err) {
                directoryEntry = readdir(directory);
                if (directoryEntry == NULL) { err = 2; break; }
                if (!strcmp(directoryEntry->d_name, ".") || !strcmp(directoryEntry->d_name, "..")) continue;
                size_t newLength = pathLength + strlen(directoryEntry->d_name) + 2;
                char *newPath = malloc(newLength);
                if (newPath == NULL) return 3;
                snprintf(newPath, newLength, "%s/%s", path, directoryEntry->d_name);
                struct stat fileInfo;
                if (!stat(newPath, &fileInfo)) {
                        if (S_ISDIR(fileInfo.st_mode)) err = data_removeDirectory(newPath);
                        else err = unlink(newPath);
                }
                free(newPath);
        }
        closedir(directory);
        rmdir(path);
        return 0;
}

char *data_getOptionsFileName (void) { return optionsFileName; }

int data_getWorldPath (char *path, const char *worldName) {
        data_ensureDirectoryExists(worldsDirectoryName);
        snprintf(path, PATH_MAX, "%s/%s", worldsDirectoryName, worldName);
        return 0;
}

void data_getWorldMetaPath (char *path, const char *worldPath) {
        snprintf(path, PATH_MAX, "%s/metadata", worldPath);
}

void data_getWorldPlayerPath (char *path, const char *worldPath, const char *name) {
        snprintf(path, PATH_MAX, "%s/%s.player", worldPath, name);
}

int data_getScreenshotPath (char *path) {
        data_ensureDirectoryExists(screenshotsDirectoryName);
        time_t unixTime = time(0);
        struct tm *timeInfo = localtime(&unixTime);
        snprintf(path, PATH_MAX, "%s/snip_%04i-%02i-%02i_%02i-%02i-%02i.bmp",
                screenshotsDirectoryName,
                timeInfo->tm_year + 1900, timeInfo->tm_mon + 1, timeInfo->tm_mday,
                timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
        return 0;
}

int data_refreshWorldList (void) {
        data_WorldListItem *item = data_worldList;
        while (item != NULL) { data_WorldListItem *next = item->next; free(item); item = next; }
        data_ensureDirectoryExists(worldsDirectoryName);
        struct dirent *directoryEntry;
        DIR *directory = opendir(worldsDirectoryName);
        if (!directory) { data_worldList = NULL; data_worldListLength = 0; return 0; }
        data_worldListLength = 0;
        data_WorldListItem *last = NULL;
        while ((directoryEntry = readdir(directory)) != NULL) {
                if (directoryEntry->d_name[0] == '.') continue;
                data_WorldListItem *newItem = calloc(sizeof(data_WorldListItem), 1);
                if (newItem == NULL) return 3;
                strncpy(newItem->name, directoryEntry->d_name, NAME_MAX);
                if (last == NULL) { data_worldList = newItem; last = data_worldList; }
                else { last->next = newItem; last = newItem; }
                char path[PATH_MAX];
                snprintf(path, PATH_MAX, "%s/%s/thumbnail.bmp", worldsDirectoryName, newItem->name);
                SDL_Surface *image = SDL_LoadBMP(path);
                int *pixel = newItem->thumbnail.buffer;
                if (image != NULL && image->h <= image->w) {
                        int scale = image->h / 16;
                        for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++) { *pixel = getSurfacePixel(image, x * scale, y * scale); pixel++; }
                } else {
                        for (int y = 0; y < 16; y++)
                        for (int x = 0; x < 16; x++) {
                                *pixel = textures[x + y * BLOCK_TEXTURE_W +
                                        (BLOCK_GRASS * 3 + 1) * BLOCK_TEXTURE_W * BLOCK_TEXTURE_H];
                                pixel++;
                        }
                }
                SDL_FreeSurface(image);
                data_worldListLength++;
        }
        closedir(directory);
        return 0;
}

static uint32_t getSurfacePixel (SDL_Surface *surface, int x, int y) {
        return *((uint32_t *) ((uint8_t *) surface->pixels
                + y * surface->pitch
                + x * surface->format->BytesPerPixel));
}