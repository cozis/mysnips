#ifndef FILE_H
#define FILE_H

#include <time.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    bool    dir;
    char  *path;
    char  *name; // The last substring of path
    size_t size;
    time_t time_create;
    time_t time_modify;
    time_t time_access;
} File;

typedef struct {
    File  *items;
    size_t count;
    size_t capacity;
} FileList;

typedef struct FileTree FileTree;
struct FileTree {

    File info;
    File head;

    FileTree *dirs;
    File    *files;
    int dirs_count;
    int files_count;
    
    bool loaded;
    bool error;
};

bool listFiles(const char *dir, FileList *list, 
               char *errmsg, size_t errmax);
void freeFileList(FileList *list);
bool queryFile(const char *path, File *file, 
               char *errmsg, size_t errmax);
bool listTree(const char *dir, int depth, FileTree *tree);
void listSubtree(const char *dir, int depth, FileTree *tree);
void freeFileTree(FileTree *tree);
#endif