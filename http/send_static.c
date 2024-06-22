#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <stdlib.h> 
#include <sys/stat.h>
#include "../http/path.h"
#include "smu.h"
#include "send_static.h"

struct {
    char *mime;
    char *ext;
} mime_table[] = {
    
    {"text/javascript", ".js"},
    {"text/javascript", ".javascript"},
    {"text/html", ".html"},
    {"text/html", ".htm"},

    {"image/gif", ".gif"},
    {"image/jpeg", ".jpg"},
    {"image/jpeg", ".jpeg"},
    {"image/svg+xml", ".svg"},
    
    {"video/mp4", ".mp4"},
    {"video/mpeg", ".mpeg"},
    
    {"font/ttf", ".ttf"},
    {"font/woff", ".woff"},
    {"font/woff2", ".woff2"},
    
    {"text/plain", ".txt"},
    
    {"audio/wav", ".wav"},
    
    {"application/x-7z-compressed", ".7z"},
    {"application/zip", ".zip"},
    {"application/xml", ".xml"},
    {"application/json", ".json"},

    {NULL, NULL},
};

char *mimetype_from_filename(char *name)
{
    size_t name_len = strlen(name);
    size_t i = 0;
    while (mime_table[i].ext) {
        char  *ext = mime_table[i].ext;
        size_t ext_len = strlen(ext);
        char  *tail = name + (name_len - ext_len);
        if (ext_len <= name_len && !memcmp(tail, ext, ext_len))
            return mime_table[i].mime;
        i++;
    }
    return NULL;
}

static bool send_dir_listing(struct server *s, uint32_t h,
                             char *dir, char *prefix, struct slice path);

/*
 * The prefix must start with / and end without it.
 */
bool serve_static_dir(char *dir,
                      char *prefix,
                      struct server *s,
                      struct request *r,
                      uint32_t handle,
                      bool dir_listing,
                      bool convert_md)
{
    assert(dir);

    size_t clean_path_len;
    char   clean_path[1<<10];

    size_t prefix_len = prefix ? strlen(prefix) : 0;
    assert(prefix_len < sizeof(clean_path));

    clean_path_len = sanitize_path(r->path.data, r->path.size, clean_path, sizeof(clean_path));
    if (clean_path_len == (size_t) -1)
        return false;

    // Return if the path doesn't have the prefix
    if (prefix_len > 0 && (clean_path_len < prefix_len || strncmp(clean_path, prefix, prefix_len)))
        return false;

    // Remove the prefix from the path and append
    // the directory.
    struct slice path;
    path.data = clean_path     + prefix_len;
    path.size = clean_path_len - prefix_len;

    size_t dir_len = dir ? strlen(dir) : 0;
    size_t tmp_len = dir_len + path.size;

    char tmp[1<<10];
    if (tmp_len >= sizeof(tmp)) {
        set_status(s, handle, 500);
        send_response(s, handle);
        return true;
    }
    if (dir)
        memcpy(tmp, dir, dir_len);
    memcpy(tmp + dir_len, path.data, path.size);
    tmp[tmp_len] = '\0';

    fprintf(stderr, "tmp=%s\n", tmp);

    bool replied;
    if (convert_md
        && tmp_len > 3
        && tmp[tmp_len-3] == '.'
        && tmp[tmp_len-2] == 'm'
        && tmp[tmp_len-1] == 'd')
        replied = send_file_md(s, handle, tmp);
    else
        replied = send_file(s, handle, tmp, NULL);

    if (!replied && dir_listing)
        replied = send_dir_listing(s, handle, dir, prefix, path);
    
    if (!replied) {
        set_status(s, handle, 404);
        send_response(s, handle);
    }

    return true;
}

int is_file(int fd)
{
    struct stat buf;
    if (fstat(fd, &buf))
        return -1;
    return S_ISREG(buf.st_mode);
}

struct markup {
    struct server *s;
    uint32_t h;
};

static void outcb(void *userp, const char *str, size_t len)
{
    struct markup *mu = userp;
    append_content(mu->s, mu->h, (char*) str, len);
}

static void outvacb(void *userp, const char *fmt, va_list args)
{
    struct markup *mu = userp;
    append_content_format_2(mu->s, mu->h, fmt, args);
}

void append_markup_as_html(struct server *s, uint32_t h, const char *str, size_t len, bool nohtml)
{
    struct markup mu = {
        .s = s,
        .h = h,
    };
    struct smu_config conf = {
        .nohtml = nohtml,
        .outfn = outcb,
        .outvafn = outvacb,
        .userp = &mu,
    };
    const char *msg;
    int ok = smu(str, len, conf, &msg);
    if (!ok) fprintf(stderr, "Error: %s\n", msg);
}

bool send_file_md(struct server *s,
                  uint32_t handle,
                  char *file)
{
    int fd;
    do
        fd = open(file, O_RDONLY);
    while (fd < 0 && errno == EINTR);

    if (fd < 0) {
        if (errno == ENOENT)
            return false;
        set_status(s, handle, 500);
    } else {
        if (!is_file(fd)) {
            close(fd);
            return false;
        }
        
        bool fail = false;
        size_t size = 0;
        size_t capacity = 1<<10;
        char *buffer = malloc(capacity);
        if (buffer) for (;;) {

            if (size == capacity) {
                size_t new_capacity = 2 * capacity;
                char *temp = realloc(buffer, new_capacity);
                if (temp == NULL) {
                    fail = true;
                    break;
                }
            }

            int num = read(fd, buffer + size, capacity - size);
            if (num < 0) {
                fail = true;
                break;
            }
            if (num == 0)
                break;

            size += num;
        }
        close(fd);

        if (fail) {
            set_status(s, handle, 500);
        } else {
            set_status(s, handle, 200);
            append_header(s, handle, "Content-Type: text/html");
            append_markup_as_html(s, handle, buffer, size, 0);
        }
        free(buffer);
    }
    send_response(s, handle);
    return true;
}

bool send_file(struct server *s,
               uint32_t handle,
               char *file,
               char *mime)
{
    int fd;
    do
        fd = open(file, O_RDONLY);
    while (fd < 0 && errno == EINTR);

    if (fd < 0) {
        if (errno == ENOENT)
            return false;
        set_status(s, handle, 500);
    } else {
        if (!is_file(fd)) {
            close(fd);
            return false;
        }
        set_status(s, handle, 200);
        if (mime == NULL) mime = mimetype_from_filename(file);
        if (mime != NULL) append_header_format(s, handle, "Content-Type: %s", mime);
        for (;;) {
            char mem[1<<13]; // 8K
            int num = read(fd, mem, sizeof(mem));
            if (num < 0)
                // Error. Unfortunately we already set the first
                // status code to 200.
                break;
            if (num == 0)
                break;
            append_content(s, handle, mem, num);
        }
        close(fd);
    }
    send_response(s, handle);
    return true;
}

static bool send_dir_listing(struct server *s, uint32_t h,
                             char *dir, char *prefix, struct slice path)
{
    // NOTE: path is assumed to be sanitized

    size_t dirlen = strlen(dir);
    char fullpath[1<<10];
    if (dirlen + path.size >= sizeof(fullpath)) {
        set_status(s, h, 500);
        send_response(s, h);
        return true;
    }
    memcpy(fullpath, dir, dirlen);
    memcpy(fullpath + dirlen, path.data, path.size);
    fullpath[dirlen + path.size] = '\0';

    DIR *d = opendir(fullpath);
    if (d == NULL) {
        if (errno == ENOENT || errno == ENOTDIR)
            return false;
        set_status(s, h, 500);
    } else {
        struct dirent *dir;
        set_status(s, h, 200);
        append_content_format(s, h,
            "<html>\n"
            "    <head>\n"
            "    </head>\n"
            "    <body>\n"
            "        <ul>\n"
            "            <li><a href=\"%s/..\">(parent)</a></li>",
            prefix, (int) path.size, path.data);
        while ((dir = readdir(d)) != NULL) {
            if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
                continue;
            append_content_format(s, h, "<li><a href=\"%s/%.*s%s\">%s</a></li>\n", prefix, (int) path.size, path.data, dir->d_name, dir->d_name);
        }
        append_content_string(s, h,
            "        </ul>\n"
            "    </body>\n"
            "</html>\n");
        closedir(d);
    }
    send_response(s, h);
    return true;
}