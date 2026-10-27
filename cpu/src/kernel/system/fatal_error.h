#ifndef FATAL_ERROR_H
#define FATAL_ERROR_H

#ifdef __ASSEMBLER__

	.extern g_fatal_active
	.extern g_fatal_frame
	.extern fatal_error_impl

#else

	#include "arm_frame.h"

	/** Fatal error callback is so we can set our own GUI hook */
	typedef void (*fatal_cb_t)(const char *, const volatile arm_frame_t *);
	extern fatal_cb_t g_fatal_cb;

	/**
	* @brief  Fatal error trap function
	*/
	void fatal_error_impl(const char *format, ...)
		__attribute__((noreturn, format(printf, 1, 2)));


	#ifdef DEBUG
	void illegal_instruction(void);
	#endif


#endif

#endif /* FATAL_ERROR_H */