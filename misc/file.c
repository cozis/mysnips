#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <SDL.h>
#include "file.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
// https://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html
#endif

typedef struct {
#ifdef _WIN32
    bool first;
    HANDLE handle;
    WIN32_FIND_DATA data;
#else
    DIR *handle;
#endif
    size_t plen;
    char buffer[PATH_MAX];
} Directory;

#ifdef _WIN32

// https://www.gamedev.net/forums/topic/565693-converting-filetime-to-time_t-on-windows/
static time_t 
filetime_to_timet(const FILETIME *ft)
{
    ULARGE_INTEGER ull;
    ull.LowPart = ft->dwLowDateTime;
    ull.HighPart = ft->dwHighDateTime;
    return ull.QuadPart / 10000000ULL - 11644473600ULL;
}

static void 
compileFileStructFromWindowsData(File *file, WIN32_FIND_DATA *data)
{
    file->dir  = (data->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    file->size = data->nFileSizeHigh * (MAXDWORD+1) + data->nFileSizeLow;
    file->time_create = filetime_to_timet(&data->ftCreationTime);
    file->time_modify = filetime_to_timet(&data->ftLastWriteTime);
    file->time_access = filetime_to_timet(&data->ftLastAccessTime);
}
#endif

bool queryFile(const char *path, File *file, 
               char *errmsg, size_t errmax)
{
    file->path = NULL;
    file->name = NULL;
#ifdef _WIN32
    WIN32_FIND_DATA data;
    HANDLE handle = FindFirstFileA(path, &data);
    if (handle == INVALID_HANDLE_VALUE) 
        goto oopsie;
    compileFileStructFromWindowsData(file, &data);
    CloseHandle(handle);
#else
    struct stat statbuf;
    int res = stat(path, &statbuf);
    if (res < 0) goto oopsie;

    file->dir  = S_ISDIR(statbuf.st_mode);
    file->size = statbuf.st_size;
    file->time_create = statbuf.st_ctime; // This isn't correct. "stat.st_ctime" refers to
                                          // the last time the inode changed, not the creation time.
    file->time_modify = statbuf.st_mtime;
    file->time_access = statbuf.st_atime;
#endif
    return true;

oopsie:
    snprintf(errmsg, errmax, "Failed to query for additional file information");
    return false;
}

#define PATH_SEP '/'

static bool 
openDirectory(const char *path, Directory *dir,
              char *errmsg, size_t errmax)
{
    size_t plen = strlen(path);
    if (plen == 0)
        return false;

    // Copy the path into the buffer of the directory
    // object making sure that it's terminated with
    // a separator.
    if (plen >= sizeof(dir->buffer)) {
        snprintf(errmsg, errmax, "Path is too long");
        return false;
    }
    memcpy(dir->buffer, path, plen);
    dir->plen = plen;
    if (path[plen-1] != PATH_SEP) {
        // Append the separator
        if (plen+1 >= sizeof(dir->buffer)) {
            snprintf(errmsg, errmax, "Path is too long");
            return false;
        }
        dir->buffer[plen] = PATH_SEP;
        dir->plen++;
    }
    
#ifdef _WIN32
    // If in windows, append a '*' to the path
    if (dir->plen+1 >= sizeof(dir->buffer)) {
        snprintf(errmsg, errmax, "Internal buffer is too small");
        return false;
    }
    dir->buffer[dir->plen+0] = '*';
    dir->buffer[dir->plen+1] = '\0';
    HANDLE find = FindFirstFile(dir->buffer, &dir->data);
    if (find == INVALID_HANDLE_VALUE) {
        snprintf(errmsg, errmax, "Couldn't open directory '%s'", path);
        return false;
    }
    dir->first  = true;
    dir->handle = find;
    return true;
#else
    DIR *dir2 = opendir(path);
    if (dir2 == NULL) {
        snprintf(errmsg, errmax, "Couldn't open directory '%s'", path);
        return false;
    }
    dir->handle = dir2;
    return true;
#endif
}

static void 
closeDirectory(Directory dir)
{
#ifdef _WIN32
    FindClose(dir.handle);
#else
    closedir(dir.handle);
#endif
}

static bool 
nextFile(Directory *dir, File *file, 
         char *errmsg, size_t errmax)
{
    char  *name;

    do {
#ifdef _WIN32
        bool found = false;

        if (dir->first) {
            dir->first = false;
            found = true;
        } else
            found = FindNextFile(dir->handle, &dir->data);

        if (!found)
            return false;

        name = dir->data.cFileName;
#else
        struct dirent *ent = readdir(dir->handle);
        if (ent == NULL)
            return false;
        name = ent->d_name;
#endif
    } while (name[0] == '.');

    size_t nlen = strlen(name);

    if (dir->plen + nlen >= sizeof(dir->buffer)) {
        snprintf(errmsg, errmax, "File path is too long");
        return false;
    }
    memcpy(dir->buffer + dir->plen, name, nlen);
    dir->buffer[dir->plen + nlen] = '\0';

    file->path = dir->buffer;
    file->name = dir->buffer + dir->plen;

#ifdef _WIN32
    compileFileStructFromWindowsData(file, &dir->data);
#else
    if (!queryFile(file->path, file, errmsg, errmax))
        return false;
#endif

    return true;
}

bool listFiles(const char *path, FileList *list, 
               char *errmsg, size_t errmax)
{
    Directory dir;
    if (!openDirectory(path, &dir, errmsg, errmax))
        return false;

    list->items = NULL;
    list->count = 0;
    list->capacity = 0;

    char  *pool = NULL;
    size_t pool_count = 0;
    size_t pool_capacity = 0;

    File file;
    int status;
    while ((status = nextFile(&dir, &file, errmsg, errmax)) > 0) {
        
        if (list->count == list->capacity) {
            size_t cap = SDL_max(8, 2 * list->capacity);
            void  *tmp = realloc(list->items, cap * sizeof(File));
            if (tmp == NULL)
                continue;
            list->items = tmp;
            list->capacity = cap;
        }

        size_t plen = strlen(file.path);

        if (pool_count + plen + 1 > pool_capacity) {
            
            // Path of the file and its null byte don't fit in the pool
            size_t cap = SDL_max(pool_count+plen+1, 2*pool_capacity);
            void  *tmp = realloc(pool, cap);
            if (tmp == NULL)
                continue;

            // Update pointers
            for (int i = 0; i < (int) list->count; i++) {
                list->items[i].path = list->items[i].path - pool + tmp;
                list->items[i].name = list->items[i].name - pool + tmp;
            }
            
            pool = tmp;
            pool_capacity = cap;
        }

        char *path = pool + pool_count;
        char *name = path + (file.name - file.path);

        memcpy(pool + pool_count, file.path, plen);
        pool[pool_count + plen] = '\0';
        pool_count += plen+1;

        file.path = path;
        file.name = name;

        list->items[list->count++] = file;
    }

    if (list->count == 0) {
        free(list->items);
        free(pool);
        list->items = NULL;
    }

    closeDirectory(dir);
    if (status < 0)
        return false;
    return true;
}

void freeFileList(FileList *list)
{
    if (list->count > 0)
        free(list->items[0].path);
    free(list->items);
}

static int 
countDirs(FileList list)
{
    int n = 0;
    for (int i = 0; i < (int) list.count; i++)
        if (list.items[i].dir)
            n++;
    return n;
}

void listSubtree(const char *dir, int depth, FileTree *tree)
{
    assert(depth >= 0);

    tree->dirs = NULL;
    tree->files = NULL;
    tree->dirs_count = 0;
    tree->files_count = 0;
    tree->error = false;
    tree->loaded = false;

    if (depth <= 0)
        return;

    char msg[1 << 9];
    FileList list;
    if (!listFiles(dir, &list, msg, sizeof(msg))) {
        tree->error = true;
        return;
    }

    tree->dirs_count = countDirs(list);
    tree->files_count = (int) list.count - tree->dirs_count;

    tree->dirs = malloc(tree->dirs_count * sizeof(FileTree));
    if (!tree->dirs) {
        tree->dirs_count = 0;
        tree->files_count = 0;
        tree->error = true;
        freeFileList(&list);
        return;
    }

    tree->files = malloc(tree->files_count * sizeof(File));
    if (!tree->files) {
        free(tree->dirs);
        tree->dirs = NULL;
        tree->dirs_count = 0;
        tree->files_count = 0;
        tree->error = true;
        freeFileList(&list);
        return;
    }

    int dirs_scanned = 0;
    int files_scanned = 0;
    for (int i = 0; i < (int) list.count; i++) {
        File file = list.items[i];

        if (i == 0) tree->head = file;

        if (file.dir) {
            FileTree *subtree = &tree->dirs[dirs_scanned++];
            subtree->info = file;
            listSubtree(file.path, depth-1, subtree);
        } else {
            tree->files[files_scanned++] = file;
        }
    }

    tree->loaded = true;
    return;
}

bool listTree(const char *dir, int depth, FileTree *tree)
{
    if (!dir || dir[0] == '\0')
        return false;

    char msg[1 << 9];

    File file;
    if (!queryFile(dir, &file, msg, sizeof(msg)))
        return false;
    if (!file.dir)
        return false;

    size_t len = strlen(dir);
    assert(len > 0);
    if (dir[len-1] == PATH_SEP)
        len--;

    char *dir_copy = malloc(len+1);
    if (!dir_copy)
        return false;
    memcpy(dir_copy, dir, len);

    size_t last_sep = len;
    while (last_sep > 0 && dir_copy[last_sep] != PATH_SEP)
        last_sep--;
    file.path = dir_copy;
    file.name = dir_copy + last_sep + 1;

    tree->info = file;

    listSubtree(file.path, depth, tree);
    return true;
}

void freeFileTree(FileTree *tree)
{
    for (int i = 0; i < tree->dirs_count; i++)
        freeFileTree(&tree->dirs[i]);
    free(tree->head.path);
    free(tree->dirs);
    free(tree->files);
}