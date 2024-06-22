/*
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

#include <stddef.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include "../common/log.h"
#include "../common/utils.h"
#include "sqlite_utils.h"

int sqlite3UtilsFetch(sqlite3_stmt *stmt, char *types, ...)
{
    va_list args;
    va_start(args, types);

    int step = sqlite3_step(stmt);
    if (step == SQLITE_DONE)
        return 1; // No more rows
    if (step != SQLITE_ROW)
        return -1; // Error occurred
    // Have a row
    for (int i = 0; types[i]; i++) {
        switch (types[i]) {
            
            case 'x': 
            *va_arg(args, const void**) = sqlite3_column_blob(stmt, i);
            *va_arg(args, size_t*) = sqlite3_column_bytes(stmt, i);
            break;
            
            case 's': *va_arg(args, const unsigned char**) = sqlite3_column_text(stmt, i); break;
            case 'i': *va_arg(args, int*) = sqlite3_column_int (stmt, i); break;
            default: va_end(args); return -1;
        }
    }
    va_end(args);
    return 0;
}

static sqlite3_stmt *vprep(sqlite3 *handle, const char *fmt, va_list args)
{
    char   buffer[1 << 10];
    size_t copied = 0;

    char params[8]; // The size of this buffer determines the maximum 
                    // number of parameters in a prepared query
    int num_params = 0;

    const char *stmt_str;
    size_t      stmt_len;

    size_t len = strlen(fmt);
    size_t cur = 0;

    while (cur < len && fmt[cur] != ':')
        cur++;

    if (cur == len) {
        stmt_str = fmt;
        stmt_len = len;
    } else {

        // The cursor refers to the first ':'

        if (cur >= sizeof(buffer)) {
            LOG(L_ERROR, "Statement text buffer is too small");
            return NULL;
        }
        
        memcpy(buffer, fmt, cur);
        copied = cur;

        do {

            assert(fmt[cur] == ':');
            cur++;
            if (cur == len) {
                LOG(L_ERROR, "Missing type specifier after ':'");
                return NULL;
            }
            
            char t = fmt[cur];
            if (t != 'i' && t != 's' && t != 'x') {
                LOG(L_ERROR, "Invalid type specifier '%c'", t);
                return NULL;
            }
            cur++;

            if (num_params == COUNT(params)) {
                LOG(L_ERROR, "Parameter limit reached (%d)", COUNT(params));
                return NULL;
            }
            params[num_params++] = t;

            if (copied+1 >= sizeof(buffer)) {
                LOG(L_ERROR, "Statement text buffer is too small");
                return NULL;
            }
            buffer[copied++] = '?';

            size_t save = cur;

            while (cur < len && fmt[cur] != ':')
                cur++;

            size_t copying = cur - save;
            if (copied + copying >= sizeof(buffer)) {
                LOG(L_ERROR, "Statement text buffer is too small");
                return NULL;
            }
            memcpy(buffer + copied, fmt + save, copying);
            copied += copying;

        } while (cur < len);

        assert(copied < sizeof(buffer));
        buffer[copied] = '\0';

        stmt_str = buffer;
        stmt_len = copied;
    }

    LOG(L_INFO, "%.*s", (int) stmt_len, stmt_str);

    sqlite3_stmt *stmt;
    int code = sqlite3_prepare_v2(handle, stmt_str, stmt_len, &stmt, 0);
    if (code != SQLITE_OK) {
        LOG(L_ERROR, "Failed to prepare SQL statement (sqlite3: %s)", sqlite3_errmsg(handle));
        return NULL;
    }

    for (int i = 0; i < num_params; i++) {
        int code;
        switch (params[i]) {
            
            case 'i': 
            {
                int v = va_arg(args, int);
                LOG(L_INFO, "binding param %d to int %d", i+1, v);
                code = sqlite3_bind_int (stmt, i+1, v); 
            }
            break;

            case 's': 
            {
                char *str = va_arg(args, char*);
                LOG(L_INFO, "binding param %d to str %s", i+1, str);
                code = sqlite3_bind_text(stmt, i+1, str, -1, NULL); 
            }
            break;
            
            case 'x': 
            {
                void  *ptr = va_arg(args, void*);
                size_t len = va_arg(args, size_t);
                LOG(L_INFO, "binding param %d to blob %p %d", i+1, ptr, (int) len);
                code = sqlite3_bind_blob(stmt, i+1, ptr, len, NULL); 
            }
            break;
        }
        if (code != SQLITE_OK) {
            LOG(L_ERROR, "Failed to bind parameter %d to SQL statement (sqlite3: %s)", i+1, sqlite3_errmsg(handle));
            sqlite3_finalize(stmt);
            return NULL;
        }
    }

    return stmt;
}

sqlite3_stmt *sqlite3UtilsPrepare(sqlite3 *handle, const char *fmt, ...)
{
    sqlite3_stmt *stmt;

    va_list args;
    va_start(args, fmt);
    stmt = vprep(handle, fmt, args);
    va_end(args);

    return stmt;
}

bool sqlite3UtilsExec(sqlite3 *handle, const char *fmt, ...)
{
    sqlite3_stmt *stmt;

    va_list args;
    va_start(args, fmt);
    stmt = vprep(handle, fmt, args);
    va_end(args);

    if (stmt == NULL)
        return false;
    
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        LOG(L_ERROR, "Failed to execute SQL statement (sqlite3: %s)", sqlite3_errmsg(handle));
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

int sqlite3UtilsRowsExist(sqlite3 *handle, const char *fmt, ...)
{
    sqlite3_stmt *stmt;

    va_list args;
    va_start(args, fmt);
    stmt = vprep(handle, fmt, args);
    va_end(args);

    if (stmt == NULL)
        return -1;

    int step = sqlite3_step(stmt);
    if (step == SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return 1; // No rows exist
    }

    if (step == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 0; // Rows exist
    }
    
    LOG(L_ERROR, "Failed to execute SQL statement (sqlite3: %s)", sqlite3_errmsg(handle));
    sqlite3_finalize(stmt);
    return -1;
}
