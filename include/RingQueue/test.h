
#ifndef _RINGQUEUE_TEST_H_
#define _RINGQUEUE_TEST_H_

#include "vs_stdint.h"

/// RingQueue������(QSIZE, ���г���, ������2���ݴη�)��Maskֵ
#define QSIZE           (1 << 10)
#define QMASK           (QSIZE - 1)

/// �ֱ���push(����)��pop(����)���߳���
#define PUSH_CNT        4
#define POP_CNT         4

/// �ַ��������̵߳���Ϣ�ܳ���, �Ǹ����߳���Ϣ�������ܺ�
#if 1
#define MSG_TOTAL_LENGTH    8000000
#else
#define MSG_TOTAL_LENGTH    80000
#endif

/// ��ͬ��MSG_TOTAL_LENGTH
#define MSG_TOTAL_CNT       (MSG_TOTAL_LENGTH)

/// �ַ���ÿ��(push)�̵߳���Ϣ����, ��ͬ��MAX_PUSH_MSG_LENGTH
#define MAX_PUSH_MSG_LENGTH (MSG_TOTAL_LENGTH / PUSH_CNT)

/// �ַ���ÿ��(pop)�̵߳���Ϣ����
#define MAX_POP_MSG_LENGTH  (MSG_TOTAL_LENGTH / POP_CNT)

/// �Ƿ������̵߳�CPU��Ե��(0������, 1����, Ĭ�ϲ�����,
///       ��ѡ����Windows����Ч, �����������ǲ�������)
#ifndef USE_THREAD_AFFINITY
#define USE_THREAD_AFFINITY     0
#endif

/// �Ƿ�����jimi:RingQueue�Ĳ��Դ���
#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE      1
#endif

/// �Ƿ�����q3.h�Ĳ��Դ���
#ifndef USE_DOUBAN_QUEUE
#define USE_DOUBAN_QUEUE        0
#endif

/// �Ƿ��������RingQueue����, ���ǽ����� RINGQUEUE_LOCK_TYPE ָ�����͵Ĳ���
/// ����Ϊ1(���0)��ʾ��������RingQueue����
#ifndef USE_FUNC_TYPE
#define USE_FUNC_TYPE           1
#endif

///
/// RingQueue�������Ͷ���: (����ú�RINGQUEUE_LOCK_TYPEδ����, ���ͬ�ڶ���Ϊ0)
///
/// ����Ϊ0, ��ʾʹ�ö�����q3.h��lock-free������,    ����RingQueue.push(), RingQueue.pop();
/// ����Ϊ1, ��ʾʹ��ϸ���ȵı�׼spin_mutex������,   ����RingQueue.spin_push(),  RingQueue.spin_pop();
/// ����Ϊ2, ��ʾʹ��ϸ���ȵĸĽ���spin_mutex������, ����RingQueue.spin1_push(), RingQueue.spin1_pop();
/// ����Ϊ3, ��ʾʹ��ϸ���ȵ�ͨ����spin_mutex������, ����RingQueue.spin2_push(), RingQueue.spin2_pop();
/// ����Ϊ4, ��ʾʹ�ô����ȵ�pthread_mutex_t��(Windows��Ϊ�ٽ���, Linux��Ϊpthread_mutex_t),
///          ����RingQueue.mutex_push(), RingQueue.mutex_pop();
/// ����Ϊ9, ��ʾʹ��ϸ���ȵķ���spin_mutex������(������), ����RingQueue.spin3_push(), RingQueue.spin3_pop();
///
/// ���� 0 ���ܻᵼ���߼�����, �������, ���ҵ�(PUSH_CNT + POP_CNT) > CPU���������ʱ,
///     �п��ܲ�����ɲ��Ի�����ʱ��ܾ�(��ʮ��򼸷��Ӳ���, ���ҽ�����Ǵ����), ��������֤.
///
/// ����ֻ��1, 2, 3, 4�����Եõ���ȷ���, 2���ٶȿ������;
///
/// 9 ���ܻ�������ţ(��Ϣ�����е����ߵú�������, ��������);
///

/// ȡֵ��Χ�� 0-9, δ������� 0
#ifndef RINGQUEUE_LOCK_TYPE
#define RINGQUEUE_LOCK_TYPE     2
#endif

/// �Ƿ���ʾ push, pop �� rdtsc �������
#define DISPLAY_PUSH_POP_DATA   1

///
/// ��spin_mutex���Ƿ�ʹ��spin_counter����, 0Ϊ��ʹ��(����!������Ϊ��ֵ), 1Ϊʹ��
///
#define USE_SPIN_MUTEX_COUNTER  0

///
/// spin_mutex�����spin_countֵ, Ĭ��ֵΪ16, ������Ϊ0��1,2, ����! ��Ϊ0���USE_SPIN_MUTEX_COUNTER��Ϊ0�ȼ�
///
#define MUTEX_MAX_SPIN_COUNT    1

#define SPIN_YIELD_THRESHOLD    1

/// �����CacheLineSize(x86����64�ֽ�)
#define CACHE_LINE_SIZE         64

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct msg_t {
    uint64_t dummy;
} msg_t;

typedef
struct spin_mutex_t {
    volatile char padding1[CACHE_LINE_SIZE];
    volatile uint32_t locked;
    volatile uint32_t spin_counter;
    volatile uint32_t recurse_counter;
    volatile uint32_t thread_id;
    volatile char padding2[CACHE_LINE_SIZE - 4 * sizeof(uint32_t)];
} spin_mutex_t;

#ifdef __cplusplus
}
#endif

#endif  /* _RINGQUEUE_TEST_H_ */
