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
#include <stdbool.h>

typedef struct GapBuffer GapBuffer;

typedef struct {
    GapBuffer *buff;
    bool crossed_gap;
    size_t cur;
    void *mem;
    char maybe[512];
} GapBufferIter;

typedef struct {
    const char *str;
    size_t len;
} GapBufferLine;

GapBuffer *GapBuffer_createUsingMemory(void *mem, size_t len, void (*free)(void*));
GapBuffer *GapBuffer_cloneUsingMemory(void *mem, size_t len, void (*free)(void*), const GapBuffer *src);
void       GapBuffer_destroy(GapBuffer *buff);
bool       GapBuffer_insertString(GapBuffer *buff, const char *str, size_t len);
void       GapBuffer_moveRelative(GapBuffer *buff, int off);
void       GapBuffer_moveAbsolute(GapBuffer *buff, size_t num);
void       GapBuffer_removeForwards(GapBuffer *buff, size_t num);
void       GapBuffer_removeBackwards(GapBuffer *buff, size_t num);
void       GapBufferIter_init(GapBufferIter *iter, GapBuffer *buff);
void       GapBufferIter_free(GapBufferIter *iter);
bool       GapBufferIter_next(GapBufferIter *iter, GapBufferLine *line);

#ifndef GAPBUFFER_NOMALLOC
GapBuffer *GapBuffer_create(size_t capacity);
bool       GapBuffer_insertStringMaybeRelocate(GapBuffer **buff, const char *str, size_t len);
#endif
