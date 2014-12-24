
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

/// �Ƿ�����q3.h�Ĳ��Դ���
#ifndef USE_DOUBAN_QUEUE
#define USE_DOUBAN_QUEUE        0
#endif

/// �Ƿ�����jimi:RingQueue�Ĳ��Դ���
#ifndef USE_JIMI_RINGQUEUE
#define USE_JIMI_RINGQUEUE      1
#endif

/// �Ƿ������̵߳�CPU��Ե��(0������, 1����, Ĭ�ϲ�����,
///       ��ѡ����Windows����Ч, �����������ǲ�������)
#ifndef USE_THREAD_AFFINITY
#define USE_THREAD_AFFINITY     0
#endif

///
/// RingQueue�������Ͷ���: (����ú�RINGQUEUE_LOCK_TYPEδ����, ���ͬ�ڶ���Ϊ0)
///
/// ����Ϊ0, ��ʾʹ�ö��������۵ĸ�����lock-free����, ����RingQueue.push(), RingQueue.pop();
/// ����Ϊ1, ��ʾʹ��ϸ���ȵı�׼spin_mutex��, ����RingQueue.spin_push(), RingQueue.spin_pop();
/// ����Ϊ2, ��ʾʹ��ϸ���ȵķ���spin_mutex��(������), ����RingQueue.spin2_push(), RingQueue.spin2_pop();
/// ����Ϊ3, ��ʾʹ�ô����ȵ�pthread_mutex_t��, ����RingQueue.locked_push(), RingQueue.locked_pop().
///
/// ����ֻ��1, 3�����Եõ���ȷ���, ����1�ٶ����;
///
/// 0���ܻᵼ���߼�����, �������, ���ҵ�(PUSH_CNT + POP_CNT) > CPU���������ʱ,
///     �п��ܲ�����ɲ��Ի�����ʱ��ܾ�(��ʮ��򼸷��Ӳ���, ���ҽ�����Ǵ����), ��������֤.
///
/// 2���ܻ�������ţ(��Ϣ�����е����ߵú�������, ��������);
///

/// ȡֵ��Χ�� 0-3
#ifndef RINGQUEUE_LOCK_TYPE
#define RINGQUEUE_LOCK_TYPE     1
#endif

///
/// ��spin_mutex���Ƿ�ʹ��spin_counter����, 0Ϊ��ʹ��(����!������Ϊ��ֵ), 1Ϊʹ��
///
#define USE_SPIN_MUTEX_COUNTER  0

///
/// spin_mutex�����spin_counterֵ, Ĭ��ֵΪ16, ������Ϊ0��1,2, ����! ��Ϊ0��USE_SPIN_MUTEX_COUNTER��Ϊ0�����ȼ�
///
#define MUTEX_MAX_SPIN_COUNTER  0

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
