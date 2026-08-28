/**
 * @file ring_buffer.h
 * @brief Generic, fixed-capacity ring buffer for single-producer /
 *        single-consumer use (e.g. main loop <-> ISR).
 *
 * Usage
 * -----
 *   uint32_t storage[16];
 *   rb_t rb;
 *
 *   ring_buffer_init(&rb, storage, sizeof(storage[0]),
 *                     sizeof(storage) / sizeof(storage[0]));
 *
 *   uint32_t sample = 42;
 *   ring_buffer_put(&rb, &sample);
 *
 *   uint32_t out;
 *   ring_buffer_get(&rb, &out);
 *
 * Thread / ISR safety
 * --------------------
 * `head` is only ever written by the producer and `tail` only ever
 * written by the consumer, so a single producer and a single consumer
 * may safely call ring_buffer_put()/ring_buffer_get() concurrently
 * (e.g. one from an ISR, the other from main context) without extra
 * locking, as long as reads/writes of `size_t` are atomic on the
 * target. Multiple producers or multiple consumers still require
 * external synchronization.
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "ft.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kept for source compatibility with users of the previous API. */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

typedef enum {
	RING_BUFFER_OK = 0,
	RING_BUFFER_ERROR_NULL_ARG,   /**< a required pointer was NULL */
	RING_BUFFER_ERROR_INVALID_SIZE, /**< elem_size == 0, or capacity
	                                     is 0 or not a power of 2 */
	RING_BUFFER_ERROR_FULL,       /**< buffer has no free slots */
	RING_BUFFER_ERROR_EMPTY,      /**< buffer has no elements */
} rb_err_t;

typedef struct {
	uint8_t *buffer;      /**< caller-owned backing storage */
	size_t elem_size;     /**< size of one element, in bytes */
	size_t capacity;      /**< number of elements; must be a power of 2 */
	volatile size_t head; /**< index of next write (producer-owned) */
	volatile size_t tail; /**< index of next read (consumer-owned) */
} rb_t;

/**
 * @brief Initialize a ring buffer over caller-supplied storage.
 *
 * @param[out] rb        Ring buffer instance to initialize.
 * @param[in]  buffer    Backing storage, at least
 *                        `elem_size * capacity` bytes, owned and
 *                        allocated by the caller (static, stack, or
 *                        heap) and kept alive for the lifetime of `rb`.
 * @param[in]  elem_size Size of one element, in bytes. Must be > 0.
 * @param[in]  capacity  Number of elements the buffer can hold. Must
 *                        be a power of 2 (e.g. 8, 16, 32, 64) and > 0.
 *
 * @return RING_BUFFER_OK on success, or an error code describing why
 *         initialization failed.
 */
rb_err_t ring_buffer_init(
	rb_t   *rb,
	void   *buffer,
	size_t  elem_size,
	size_t  capacity
);

/**
 * @brief Add an element to the buffer.
 * @return RING_BUFFER_OK, RING_BUFFER_ERROR_NULL_ARG, or
 *         RING_BUFFER_ERROR_FULL if there is no room.
 */
rb_err_t ring_buffer_put(
	rb_t       *rb,
	const void *elem
);

/**
 * @brief Add an element to the buffer, overwriting the oldest element
 *        if the buffer is full instead of failing.
 *
 * @param[out] overwritten Optional (may be NULL). Set to true if an
 *                          existing element was discarded to make room.
 * @return RING_BUFFER_OK on success (this call cannot fail with
 *         "full"), or RING_BUFFER_ERROR_NULL_ARG.
 */
rb_err_t ring_buffer_put_overwrite(
	rb_t       *rb,
	const void *elem,
	bool       *overwritten
);

/**
 * @brief Remove and return the oldest element in the buffer.
 * @return RING_BUFFER_OK, RING_BUFFER_ERROR_NULL_ARG, or
 *         RING_BUFFER_ERROR_EMPTY if there is nothing to read.
 */
rb_err_t ring_buffer_get(
	rb_t *rb,
	void *elem
);

/**
 * @brief Copy an element without removing it from the buffer.
 * @param[in] offset Number of elements after the oldest element.
 */
rb_err_t ring_buffer_peek(
	const rb_t *rb,
	size_t      offset,
	void       *elem
);

/** @brief Discard up to `count` oldest elements. */
void ring_buffer_discard(rb_t *rb, size_t count);

/** @brief True if the buffer has no free slots. */
bool ring_buffer_is_full(const rb_t *rb);

/** @brief True if the buffer has no elements. */
bool ring_buffer_is_empty(const rb_t *rb);

/** @brief Number of elements currently stored. */
size_t ring_buffer_count(const rb_t *rb);

/** @brief Maximum number of elements the buffer can hold. */
size_t ring_buffer_capacity(const rb_t *rb);

/** @brief Discard all elements. Does not touch the backing storage. */
void ring_buffer_reset(rb_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
