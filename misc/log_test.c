#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "log.h"
#include "../thread/thread.h"

#if defined(__linux__)
#include <unistd.h>
#endif

#define NUM_THREADS 1
#define NUM_PRINTS_PER_THREAD 1000

#define MSG "|------------------\n"

os_threadreturn func(void*)
{
    for (int i = 0; i < NUM_PRINTS_PER_THREAD; i++)
        log_write(MSG);
    return 0;
}

int main(void)
{
    char file[] = "log_test.txt";
    if (unlink(file) && errno != ENOENT) abort();

    log_init(file);
    os_thread desc[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        os_thread_create(&desc[i], NULL, func);
    for (int i = 0; i < NUM_THREADS; i++)
        os_thread_join(desc[i]);
    log_quit();

    {
        FILE *stream = fopen(file, "rb");
        if (!stream) abort();

        fseek(stream, 0, SEEK_END);
        long size = ftell(stream);
        fseek(stream, 0, SEEK_SET);

        size_t expected = NUM_THREADS * NUM_PRINTS_PER_THREAD * (sizeof(MSG)-1);

        if (expected != (size_t) size) {
            fprintf(stderr, "FAILED: More bytes than expected (%d) were written (%d)\n", (int) expected, (int) size);
            abort();
        }

        for (int i = 0; i < NUM_THREADS * NUM_PRINTS_PER_THREAD; i++) {
            char buffer[sizeof(MSG)-1];
            size_t num = fread(buffer, 1, sizeof(buffer), stream);
            if (num != sizeof(buffer)) {
                fprintf(stderr, "FAILED: Expected %d bytes but read %d\n", (int) sizeof(buffer), (int) num);
                abort();
            }
            if (memcmp(buffer, MSG, sizeof(MSG)-1)) {
                fprintf(stderr, "FAILED: Expected [%.*s] got [%.*s]\n", (int) sizeof(MSG)-1, MSG, (int) num, buffer);
                abort();
            }
        }

        fprintf(stderr, "PASSED\n");
        fclose(stream);
    }
    return 0;
}
