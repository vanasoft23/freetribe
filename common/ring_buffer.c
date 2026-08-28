#include "ft.h"

#include <string.h>

#include "ring_buffer.h"



static size_t slot_offset(const rb_t *rb, size_t index)
{
	return (index & (rb->capacity - 1)) * rb->elem_size;
}

rb_err_t ring_buffer_init(rb_t *rb, void *buffer, size_t elem_size, size_t capacity)
{
	if ((rb == NULL) || (buffer == NULL)) {
		return RING_BUFFER_ERROR_NULL_ARG;
	}

	/* capacity must be a power of 2 (0 doesn't count) so that indices
	 * can be wrapped with a bitmask instead of a modulo */
	if ((elem_size == 0) || (capacity == 0) || ((capacity & (capacity - 1)) != 0)) {
		return RING_BUFFER_ERROR_INVALID_SIZE;
	}

	rb->buffer = (uint8_t *)buffer;
	rb->elem_size = elem_size;
	rb->capacity = capacity;
	rb->head = 0;
	rb->tail = 0;

	return RING_BUFFER_OK;
}

rb_err_t ring_buffer_put(rb_t *rb, const void *elem)
{
	if ((rb == NULL) || (elem == NULL)) {
		return RING_BUFFER_ERROR_NULL_ARG;
	}

	if (ring_buffer_is_full(rb)) {
		return RING_BUFFER_ERROR_FULL;
	}

	memcpy(&rb->buffer[slot_offset(rb, rb->head)], elem, rb->elem_size);
	rb->head++;

	return RING_BUFFER_OK;
}

rb_err_t ring_buffer_put_overwrite(rb_t *rb, const void *elem, bool *overwritten)
{
	if ((rb == NULL) || (elem == NULL)) {
		return RING_BUFFER_ERROR_NULL_ARG;
	}

	const bool was_full = ring_buffer_is_full(rb);
	if (was_full) {
		rb->tail++; /* drop the oldest element to make room */
	}
	if (overwritten != NULL) {
		*overwritten = was_full;
	}

	memcpy(&rb->buffer[slot_offset(rb, rb->head)], elem, rb->elem_size);
	rb->head++;

	return RING_BUFFER_OK;
}

rb_err_t ring_buffer_get(rb_t *rb, void *elem)
{
	if ((rb == NULL) || (elem == NULL)) {
		return RING_BUFFER_ERROR_NULL_ARG;
	}

	if (ring_buffer_is_empty(rb)) {
		return RING_BUFFER_ERROR_EMPTY;
	}

	memcpy(elem, &rb->buffer[slot_offset(rb, rb->tail)], rb->elem_size);
	rb->tail++;

	return RING_BUFFER_OK;
}

rb_err_t ring_buffer_peek(const rb_t *rb, size_t offset, void *elem)
{
	if ((rb == NULL) || (elem == NULL)) {
		return RING_BUFFER_ERROR_NULL_ARG;
	}

	if (offset >= ring_buffer_count(rb)) {
		return RING_BUFFER_ERROR_EMPTY;
	}

	memcpy(elem, &rb->buffer[slot_offset(rb, rb->tail + offset)], rb->elem_size);
	return RING_BUFFER_OK;
}

void ring_buffer_discard(rb_t *rb, size_t count)
{
	if (rb == NULL) {
		return;
	}

	const size_t pending = ring_buffer_count(rb);
	if (count > pending) {
		count = pending;
	}

	rb->tail += count;
}

bool ring_buffer_is_full(const rb_t *rb)
{
	return (rb != NULL) && ((rb->head - rb->tail) == rb->capacity);
}

bool ring_buffer_is_empty(const rb_t *rb)
{
	return (rb == NULL) || (rb->head == rb->tail);
}

size_t ring_buffer_count(const rb_t *rb)
{
	return (rb != NULL) ? (rb->head - rb->tail) : 0;
}

size_t ring_buffer_capacity(const rb_t *rb)
{
	return (rb != NULL) ? rb->capacity : 0;
}

void ring_buffer_reset(rb_t *rb)
{
	if (rb != NULL) {
		rb->head = 0;
		rb->tail = 0;
	}
}
