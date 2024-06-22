#include <stdlib.h>
#include "thread_pool.h"

struct lock_free_queue {

    _Atomic(int) head;
    _Atomic(int) tail;
    _Atomic(int) temp;

    // The following aren't written to by
    // push and pop operations, therefore
    // don't need to be atomic.
    int   size; // Capacity of the queue (item count)
    int   cell; // Size of a single item (in bytes)
    char *base; // Pointer to the first item
};

static void
lock_free_queue_init(struct lock_free_queue *q, 
                     void *arrptr, int arrlen,
                     int cell)
{
    assert(arrlen % cell == 0);

    q->head = 0;
    q->tail = 0;
    q->temp = 0;
    q->size = arrlen / cell;
    q->base = arrptr;
    q->cell = cell;
}

static void*
item_addr(struct lock_free_queue *q, int index)
{
    assert(index >= 0 && index < q->size);
    return q->base + q->cell * index;
}

static int
acquire_push_location(struct lock_free_queue *q)
{
    int old_temp;
    int new_temp;
    do {
        old_temp = q->temp;
        new_temp = (old_temp + 1) % q->size;
    } while (!atomic_compare_exchange_strong(&q->temp, &old_temp, new_temp));
    return old_temp;
}

static void 
release_push_location(struct lock_free_queue *q, int index)
{
    // The current thread already inserted an
    // item at position "index", which comes
    // after the queue's tail.
    //
    // It may be possible that other threads
    // inserted items before this one, buf still
    // after the tail
    //
    // Before being able to move the tail over
    // this element, we need to wait for other
    // threads to do it.
 
    int old_tail;
    int new_tail;
    do {
        old_tail = q->tail;
        new_tail = (old_tail + 1) % q->size;
    } while (!atomic_compare_exchange_weak(&q->tail, &old_tail, new_tail));
}

static bool
lock_free_queue_try_push(struct lock_free_queue *q, void *src)
{
    int index = acquire_push_location(q);
    if (index < 0) return false;

    void *dst = item_addr(q, index);
    memcpy(dst, src, q->cell);

    release_push_location(q, index);
    return true;
}

static void
lock_free_queue_push(struct lock_free_queue *q, void *src)
{
    while (!lock_free_queue_try_push(q, src));
}

static bool
lock_free_queue_try_pop(struct lock_free_queue *q, void *dst)
{
    int old_head;
    int new_head;
    do {
        old_head = q->head;
        new_head = (old_head + 1) % q->size;
        void *src = item_addr(q, old_head);
        memcpy(dst, src, q->cell);
    } while (!atomic_compare_exchange_strong(&q->head, &old_head, new_head));
    return true;
}

static void
lock_free_queue_pop(struct lock_free_queue *q, void *dst)
{
    while (!lock_free_queue_try_pop(q, dst));
}


#define PACK_JOB_ID(gen, idx) ((uint64_t) ((gen)+1) | ((uint64_t) ((idx)+1) << 32))
#define UNPACK_GEN_FROM_JOB_ID(job_id) (uint32_t) (((uint64_t) (job_id) & 0xffffffff)-1)
#define UNPACK_IDX_FROM_JOB_ID(job_id) (uint32_t) (((uint64_t) (job_id) >> 32)-1)

static job_id_t 
get_job_id(struct thread_pool *pool, struct job_data *job)
{
    int gen = job->gen;
    int idx = job - pool->jobs;
    return PACK_JOB_ID(gen, idx);
}

static struct job_data*
get_job_data(struct thread_pool *pool, job_id_t job_id)
{
    if (job_id == INVALID_JOB)
        return NULL;
    
    uint32_t idx = UNPACK_IDX_FROM_JOB_ID(job_id);
    uint32_t gen = UNPACK_GEN_FROM_JOB_ID(job_id);

    // Check index
    if (idx >= (uint32_t) pool->max_jobs)
        return NULL;
    
    struct job_data *job = &pool->jobs[idx];

    if (job->gen != gen)
        return NULL;
    
    return job;
}

static void 
free_list_push(struct thread_pool *pool,
               struct job_data *job)
{
    struct job_data *old_head;
    struct job_data *new_head;
    do {
        old_head = pool->free_jobs;
        new_head = job;
        job->next = old_head;
    } while (!atomic_compare_exchange_strong(&pool->free_jobs, &old_head, new_head));
}

static struct job_data*
free_list_pop(struct thread_pool *pool)
{
    struct job_data *job;
    struct job_data *old_head;
    struct job_data *new_head;
    do {
        old_head = pool->free_jobs;
        new_head = old_head->next;
        job = old_head;
    } while (!atomic_compare_exchange_strong(&pool->free_jobs, &old_head, new_head));
    return job;
}

static job_id_t create_job(struct thread_pool *pool, void *data, 
                           void (*func)(void *data), bool join)
{
    struct job_data *job;
    
    // Check that the system can hold a new job.
    //
    // If the system is full and each job is joinable,
    // then no amount of waiting will allow this thread
    // to get a job structure.
    if (pool->joinable_count == pool->max_jobs)
        return INVALID_JOB;

    // Wait for a free job structure
    tinycsem_wait(&pool->complete);
    job = free_list_pop(pool);
    assert(job);
    
    // Init the job structure
    job->done = false;
    job->data = data;
    job->func = func;
    job->join = join;
    if (join) {
        if (!tinycsem_init(&job->sem, 0, 1)) {
            free_list_push(pool, job);
            tinycsem_signal(&pool->complete);
            return INVALID_JOB;
        }
        pool->joinable_count++;
    }

    // Push the index of the job into the lock-free
    // queue. This queue will wait actively until
    // a space is available in the queue. Although
    // there should be no wait since we know a space
    // is available at this point. (The queue's capacity
    // equals the number of jobs)
    int index = job - pool->jobs;
    lock_free_queue_push(&pool->queue, &index);

    // Tell the workers a new job is available
    tinycsem_signal(&pool->pending);
    return get_job_id(pool, job);
}

job_id_t basic_thread_pool_run(struct thread_pool *pool, void *data, 
                               void (*func)(void *data))
{
    return create_job(pool, data, func, true);
}

bool basic_thread_pool_run_2(struct thread_pool *pool, void *data, 
                             void (*func)(void *data))
{
    return create_job(pool, data, func, false) != INVALID_JOB;
}

bool basic_thread_pool_done(struct thread_pool *pool, job_id_t job_id)
{
    struct job_data *job = get_job_data(pool, job_id);
    if (job == NULL) return false;

    return job->done;
}

static void free_job(struct thread_pool *pool, 
                     struct job_data *job)
{
    tinycsem_free(&job->sem);

    // Update the generation counter to invalidate
    // any handle that's currently being held.
    job->gen++;

    free_list_push(pool, job);
}

bool basic_thread_pool_wait(struct thread_pool *pool, job_id_t job_id)
{
    struct job_data *job = get_job_data(pool, job_id);
    if (job == NULL) return false; // Invalid job_id

    if (!tinycsem_wait(&job->sem))
        return false;
    
    // Job completed. It can be freed now.

    free_job(pool, job);
    tinycsem_signal(&pool->complete);
    pool->joinable_count--;
    return true;
}

static int worker_routine(void *arg)
{
    struct thread_pool *pool = arg;

    for (;;) {

        tinycsem_wait(&pool->pending);

        int index;
        lock_free_queue_pop(&pool->queue, &index);

        if (index < 0)
            break; // If we popped an invalid index, the thread 
                   // manager wants us to stop.

        struct job_data *job = &pool->jobs[index];

        job->func(job->data);

        if (job->join) {
            job->done = true;
            tinycsem_signal(&job->sem);
        } else {
            free_job(pool, job);
            tinycsem_signal(&pool->complete);
        }
    }
    return 0;
}

static void
ensure_deinit(struct thread_pool *pool)
{
    // Push one invalid index onto the queue
    // per worker. When a worker pop them, they
    // will stop execution.
    for (int i = 0; i < pool->num_workers; i++) {
        int value = -1;
        lock_free_queue_push(&pool->queue, &value);
    }
    
    // Tell the workers there is something to pop.
    for (int i = 0; i < pool->num_workers; i++)
        tinycsem_signal(&pool->pending);
    
    // Wait for them
    for (int i = 0; i < pool->num_workers; i++)
        thrd_join(pool->workers[i], NULL);
    
    tinycsem_free(&pool->pending);
    tinycsem_free(&pool->complete);
}

static bool
ensure_init(struct thread_pool *pool, int *indices, 
            struct job_data *jobs, int max_jobs, 
            thrd_t *workers, int num_workers)
{
    pool->jobs = jobs;
    pool->max_jobs = max_jobs;
    pool->joinable_count = 0;

    // Build the free list
    for (int i = 0; i < max_jobs-1; i++) {
        jobs[i].gen = 0;
        jobs[i].next = &jobs[i+1];
    }
    jobs[max_jobs-1].next = NULL;
    pool->free_jobs = jobs;

    lock_free_queue_init(&pool->queue, indices, max_jobs * sizeof(indices[0]), sizeof(indices[0]));

    if (!tinycsem_init(&pool->pending, 0, max_jobs))
        return false;
    if (!tinycsem_init(&pool->complete, max_jobs, max_jobs))
        return false;

    int created = 0;
    for (int i = 0; i < num_workers; i++) {
        if (thrd_create(&workers[i], worker_routine, pool) == thrd_success)
            created++;
    }

    if (created == 0) {
        tinycsem_free(&pool->pending);
        tinycsem_free(&pool->complete);
        return false; // Couldn't create even one worker
    }

    if (created < num_workers) {
        // Less workers than expected were created
    }

    pool->workers = workers;
    pool->num_workers = created;
    return true;
}

#define MAX_WORKERS 32

bool g_init = false;

static struct job_data *g_jobs;
static int *g_indices;

static thrd_t g_workers[MAX_WORKERS];

static struct thread_pool g_pool;

bool job_system_init(int max_workers, int max_jobs)
{
    if (g_init) return false;
    if (max_jobs < 0) return false;
    if (max_workers < 0) return false;
    if (max_workers > MAX_WORKERS) max_workers = MAX_WORKERS;

    void *mem = malloc(max_jobs * (sizeof(struct job_data) + sizeof(int)));
    if (!mem) return false;

    g_jobs = mem;
    g_indices = (int*) (g_jobs + max_jobs);

    if (!basic_thread_pool_init(&g_pool, g_indices, g_jobs, max_jobs, g_workers, max_workers)) {
        free(mem);
        return false;
    }

    g_init = true;
    return true;
}

void job_system_free(void)
{
    if (!g_init)
        return;
    basic_thread_pool_free(&g_pool);
    free(g_jobs);
    g_init = false;
}

uint64_t job_run(void *data, void (*func)(void*))
{
    if (!g_init) return INVALID_JOB;
    return basic_thread_pool_run(&g_pool, data, func);
}

bool job_run2(void *data, void (*func)(void*))
{
    if (!g_init) return false;
    return basic_thread_pool_run_2(&g_pool, data, func);
}

void job_wait(uint64_t id)
{
    basic_thread_pool_wait(&g_pool, id);
}

bool job_done(uint64_t id)
{
    return basic_thread_pool_done(&g_pool, id);
}