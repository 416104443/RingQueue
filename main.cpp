
#define _GNU_SOURCE
#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sched.h>
#include <pthread.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "test.h"

#include "q3.h"
//#include "q.h"
//#include "qlock.h"

#include "get_char.h"
#include "console.h"
#include "RingQueue.h"

using namespace jimi;

#if defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32)
typedef unsigned int cpu_set_t;
#endif // defined

typedef RingQueue<msg_t, QSIZE> RingQueue_t;

typedef struct thread_arg_t
{
    int         idx;
    RingQueue_t *queue;
} thread_arg_t;

static volatile struct msg_t *msgs;
//static volatile struct msg_t *popmsg_list;

static struct msg_t *popmsg_list[POP_CNT][MAX_POP_MSG_LENGTH];

static pthread_mutex_t s_queue_mutex = NULL;

static inline uint64_t
rdtsc(void)
{
  union {
    uint64_t tsc_64;
    struct {
      uint32_t lo_32;
      uint32_t hi_32;
    };
  } tsc;

  asm volatile("rdtsc" :
    "=a" (tsc.lo_32),
    "=d" (tsc.hi_32));
  return tsc.tsc_64;
}

static volatile int quit = 0;
static volatile int pop_total = 0;
static volatile int push_total = 0;

static volatile uint64_t push_cycles = 0;
static volatile uint64_t pop_cycles = 0;

static void
init_globals(void)
{
    pthread_mutexattr_t attr;
    quit = 0;
    pop_total = 0;
    push_total = 0;

    push_cycles = 0;
    pop_cycles = 0;

    if (s_queue_mutex == NULL) {
        if (!pthread_mutexattr_init(&attr)) {
            pthread_mutex_init(&s_queue_mutex, NULL);
        }
    }
}

static void *
ringqueue_push_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue_t *queue;
    struct msg_t *msg;
    uint64_t start;
    int i, idx;

    idx = 0;
    queue = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx   = thread_arg->idx;
        queue = (RingQueue_t *)thread_arg->queue;
    }

    if (queue == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    msg = (struct msg_t *)&msgs[idx * MAX_PUSH_MSG_LENGTH];
    start = rdtsc();

    for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
        while (queue->spin_push(msg) == -1);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
        while (queue->spin2_push(msg) == -1);
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
        while (queue->locked_push(msg) == -1);
#else
        while (queue->push(msg) == -1);
#endif
        msg++;
#if 0
        //if ((i & 0x3FFFU) == 0x3FFFU) {
        if ((i & 0xFFFFU) == 0xFFFFU) {
            printf("thread [%d] have push %d\n", idx, i);
        }
#endif
    }

#if 0
    printf("thread [%d] have push %d\n", idx, i);
#endif

    push_cycles += rdtsc() - start;
    push_total += MAX_PUSH_MSG_LENGTH;
    if (push_total == MSG_TOTAL_CNT)
        quit = 1;

    return NULL;
}

static void *
ringqueue_pop_task(void *arg)
{
    thread_arg_t *thread_arg;
    RingQueue_t *queue;
    struct msg_t *msg;
    struct msg_t **record_list;
    uint64_t start;
    int idx;
    int cnt = 0;

    idx = 0;
    queue = NULL;

    thread_arg = (thread_arg_t *)arg;
    if (thread_arg) {
        idx   = thread_arg->idx;
        queue = (RingQueue_t *)thread_arg->queue;
    }

    if (queue == NULL)
        return NULL;

    if (thread_arg)
        free(thread_arg);

    cnt = 0;
    record_list = &popmsg_list[idx][0];
    start = rdtsc();

    while (true || !quit) {
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
        msg = (struct msg_t *)queue->spin_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
        msg = (struct msg_t *)queue->spin2_pop();
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
        msg = (struct msg_t *)queue->locked_pop();
#else
        msg = (struct msg_t *)queue->pop();
#endif
        if (msg != NULL) {
            *record_list++ = (struct msg_t *)msg;
            cnt++;
            if (cnt >= MAX_POP_MSG_LENGTH)
                break;
        }
    }

    pop_cycles += rdtsc() - start;
    pop_total += cnt;

    return NULL;
}

static void *
push_task(void *arg)
{
    struct queue *q = (struct queue *)arg;
    uint64_t start = rdtsc();
    int i;

    for (i = 0; i < MAX_PUSH_MSG_LENGTH; i++) {
        while (push(q, msgs + i) == -1);
    }

    push_cycles += rdtsc() - start;
    push_total += MAX_PUSH_MSG_LENGTH;
    if (push_total == MSG_TOTAL_CNT)
        quit = 1;

    return NULL;
}

static void *
pop_task(void *arg)
{
    struct queue *q = (struct queue *)arg;
    uint64_t start = rdtsc();
    int cnt = 0;

    while (!quit) {
        cnt += !!pop(q);
    }

    pop_cycles += rdtsc() - start;
    pop_total += cnt;

    return NULL;
}

/* topology for Xeon E5-2670 Sandybridge */
static const int socket_top[] = {
  1,  2,  3,  4,  5,  6,  7,
  16, 17, 18, 19, 20, 21, 22, 23,
  8,  9,  10, 11, 12, 13, 14, 15,
  24, 25, 26, 27, 28, 29, 30, 31
};

#define CORE_ID(i)    socket_top[(i)]

static int
start_thread(int id,
       void *(*cb)(void *),
       void *arg,
       pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    int core_id;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    if (pthread_attr_init(&attr))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    CPU_ZERO(&cpuset);
    core_id = CORE_ID(id);
    CPU_SET(core_id, &cpuset);
#endif // defined

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif

    if (tid)
        *tid = kid;

    return 0;
}

static int
setaffinity(int core_id)
{
#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    cpu_set_t cpuset;
    pthread_t me = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    if (pthread_setaffinity_np(me, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif
    return 0;
}

static int
ringqueue_start_thread(int id,
                       void *(*cb)(void *),
                       void *arg,
                       pthread_t *tid)
{
    pthread_t kid;
    pthread_attr_t attr;
    cpu_set_t cpuset;
    int core_id;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (id < 0 || id >= sizeof(socket_top) / sizeof(int))
        return -1;
#endif

    if (pthread_attr_init(&attr))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    CPU_ZERO(&cpuset);
    core_id = CORE_ID(id);
    CPU_SET(core_id, &cpuset);
#endif // defined

    if (pthread_create(&kid, &attr, cb, arg))
        return -1;

#if !(defined(__MINGW__) || defined(__MINGW32__) || defined(__CYGWIN__))
    if (pthread_setaffinity_np(kid, sizeof(cpu_set_t), &cpuset))
        return -1;
#endif

    if (tid)
        *tid = kid;

    return 0;
}

int verify_pop_list(void)
{
    int i, j;
    uint32_t index;
    uint32_t *verify_list;
    int empty, overlay, correct, errors, times;

    verify_list = (uint32_t *)calloc(MSG_TOTAL_CNT, sizeof(uint32_t));
    if (verify_list == NULL)
        return -1;

    for (i = 0; i < POP_CNT; ++i) {
        for (j = 0; j < MAX_PUSH_MSG_LENGTH; ++j) {
            index = (uint32_t)(popmsg_list[i][j]->dummy - 1);
            if (index < MSG_TOTAL_CNT)
                verify_list[index] = verify_list[index] + 1;
        }
    }

    empty = 0;
    overlay = 0;
    correct = 0;
    errors = 0;
    for (i = 0; i < MSG_TOTAL_CNT; ++i) {
        times = verify_list[i];
        if (times == 0) {
            empty++;
        }
        else if (times > 1) {
            overlay++;
            if (times >= 3) {
                if (errors == 0)
                    printf("Serious Errors:\n");
                errors++;
                printf("verify_list[%8d] = %d\n", i, times);
            }
        }
        else {
            correct++;
        }
    }

    if (errors > 0)
        printf("\n");
    printf("verify pop list result:\n\n");
    printf("empty = %d, overlay = %d, correct = %d, total = %d\n\n",
           empty, overlay, correct, empty + overlay + correct);

    if (verify_list)
        free(verify_list);

    //jimi_console_readkeyln(false, true, false);

    return correct;
}

void
RingQueue_Test(void)
{
    RingQueue_t ringQueue(true, true);
    int i, j;
    pthread_t kids[POP_CNT + PUSH_CNT];
    thread_arg_t *thread_arg;

    printf("\n");
#if defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 1)
    printf("This is RingQueue.spin_push() test:\n");
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 2)
    printf("This is RingQueue.spin2_push() test (maybe deadlock):\n");
#elif defined(RINGQUEUE_LOCK_TYPE) && (RINGQUEUE_LOCK_TYPE == 3)
    printf("This is RingQueue.locked_push() test:\n");
#else
    printf("This is RingQueue.push() test (modified by q3.h):\n");
#endif

    init_globals();
    setaffinity(0);

    msgs = (struct msg_t *)calloc(MSG_TOTAL_CNT, sizeof(struct msg_t));

    for (i = 0; i < MSG_TOTAL_CNT; i++)
        msgs[i].dummy = (uint64_t)(i + 1);

    for (i = 0; i < POP_CNT; i++) {
        for (j = 0; j < MAX_POP_MSG_LENGTH; ++j) {
            popmsg_list[i][j] = NULL;
        }
    }

    for (i = 0; i < PUSH_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->queue = &ringQueue;
        ringqueue_start_thread(i, ringqueue_push_task, (void *)thread_arg,
                               &kids[i]);
    }
    for (i = 0; i < POP_CNT; i++) {
        thread_arg = (thread_arg_t *)malloc(sizeof(struct thread_arg_t));
        thread_arg->idx = i;
        thread_arg->queue = &ringQueue;
        ringqueue_start_thread(i + PUSH_CNT, ringqueue_pop_task, (void *)thread_arg,
                               &kids[i + PUSH_CNT]);
    }
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    printf("\n");
    printf("pop total: %d\n", pop_total);
    printf("pop cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
    printf("push total: %d\n", push_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MSG_TOTAL_CNT);
    printf("\n");

    printf("msgs ptr = 0x%08p\n\n", (struct msg_t *)msgs);

    //jimi_console_readkeyln(false, true, false);

    verify_pop_list();

#if 0
    for (j = 0; j < POP_CNT; ++j) {
        for (i = 0; i <= 256; ++i) {
            printf("pop_list[%2d, %3d] = ptr: 0x%08p, %02"PRIuFAST64" : %"PRIuFAST64"\n", j, i,
                   (struct msg_t *)(popmsg_list[j][i]),
                   popmsg_list[j][i]->dummy / (MAX_PUSH_MSG_LENGTH),
                   popmsg_list[j][i]->dummy % (MAX_PUSH_MSG_LENGTH));
        }
        printf("\n");
        if (j < (POP_CNT - 1)) {
            jimi_console_readkeyln(false, true, false);
        }
    }
    printf("\n");
#endif

    //jimi_console_readkeyln(false, true, false);

#if 0
    for (i = 0; i <= 256; ++i) {
        printf("msgs[%3d] = %02llu : %llu\n", i, msgs[i].dummy / MAX_PUSH_MSG_LENGTH,
               msgs[i].dummy % MAX_PUSH_MSG_LENGTH);
    }
    printf("\n");
#endif

    if (msgs) {
        free((void *)msgs);
        msgs = NULL;
    }

    //getchar();
    jimi_console_readkeyln(true, true, false);
}

void
q3_test(void)
{
    struct queue *q = qinit();
    int i;
    pthread_t kids[POP_CNT + PUSH_CNT];

    printf("\n");
    printf("This is DouBan's q3.h test:\n");

    init_globals();
    setaffinity(0);

    msgs = (struct msg_t *)calloc(MSG_TOTAL_CNT, sizeof(struct msg_t));

    for (i = 0; i < MSG_TOTAL_CNT; i++)
        msgs[i].dummy = (uint64_t)(i + 1);

    for (i = 0; i < PUSH_CNT; i++)
        start_thread(i, push_task, q, &kids[i]);
    for (i = 0; i < POP_CNT; i++)
        start_thread(i + PUSH_CNT, pop_task, q, &kids[i + PUSH_CNT]);
    for (i = 0; i < POP_CNT + PUSH_CNT; i++)
        pthread_join(kids[i], NULL);

    printf("\n");
    printf("pop total: %d\n", pop_total);
    printf("pop cycles/msg: %"PRIuFAST64"\n", pop_cycles / pop_total);
    printf("push total: %d\n", push_total);
    printf("push cycles/msg: %"PRIuFAST64"\n", push_cycles / MSG_TOTAL_CNT);
    printf("\n");

    if (msgs) {
        free((void *)msgs);
        msgs = NULL;
    }

    //getchar();
    jimi_console_readkeyln(false, true, false);
}

void
RingQueue_UnitTest(void)
{
    RingQueue_t ringQueue(true, true);
    msg_t queue_msg = { 123ULL };

    init_globals();

    printf("---------------------------------------------------------------\n");
    printf("RingQueue2() test begin...\n\n");

    printf("ringQueue.capcity() = %u\n", ringQueue.capcity());
    printf("ringQueue.mask()    = %u\n\n", ringQueue.mask());
    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.dump_detail();

    ringQueue.push(&queue_msg);
    ringQueue.dump_detail();
    ringQueue.push(&queue_msg);
    ringQueue.dump_detail();

    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.pop();
    ringQueue.dump_detail();
    ringQueue.pop();
    ringQueue.dump_detail();

    ringQueue.pop();
    ringQueue.dump_detail();

    printf("ringQueue.sizes()   = %u\n\n", ringQueue.sizes());

    ringQueue.dump_info();

    printf("RingQueue2() test end...\n");
    printf("---------------------------------------------------------------\n\n");

    //getchar();
    jimi_console_readkeyln(true, true, false);
}

void test_data_destory(void)
{
    if (s_queue_mutex) {
        pthread_mutex_destroy(&s_queue_mutex);
    }
}

int
main(void)
{
#if defined(USE_JIMI_RINGQUEUE) && (USE_JIMI_RINGQUEUE != 0)
    //RingQueue_UnitTest();
    RingQueue_Test();
#endif

#if defined(USE_DOUBAN_RINGQUEUE) && (USE_DOUBAN_RINGQUEUE != 0)
    q3_test();
#endif

    test_data_destory();

    //jimi_console_readkeyln(false, true, false);
    return 0;
}
