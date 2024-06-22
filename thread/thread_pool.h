#ifndef COZIS_THREAD_POOL_H
#define COZIS_THREAD_POOL_H

#include <cozis/types.h>

typedef u64 task_id_t;
typedef void (*task_t)(void*);

bool      init_thread_pool(int max_workers, int task_limit);
void      free_thread_pool(void);
task_id_t async_run(void *data, task_t func);
bool      async_run_and_forget(void *data, task_t func);
void      wait_for_func(task_id_t id);
bool      has_func_done(task_id_t id);

#endif
