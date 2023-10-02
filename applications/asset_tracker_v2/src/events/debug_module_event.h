/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _DEBUG_MODULE_EVENT_H_
#define _DEBUG_MODULE_EVENT_H_

/**
 * @brief Debug module event
 * @defgroup debug_module_event Debug module event
 * @{
 */

#include <app_event_manager.h>
#include <app_event_manager_profiler_tracer.h>

#ifdef __cplusplus
extern "C" {
#endif

enum debug_module_event_type {
	/** Event sent after boot when building for PC. This event acts as a placeholder for
	 *  MODEM_EVT_INITIALIZED which is not sent due to the modem module being disabled for
	 *  PC builds.
	 */
	DEBUG_EVT_EMULATOR_INITIALIZED,

	/** Event sent when the application is built for PC.
	 *  When built for PC it is assumed that the application
	 *  is connected to the network.
	 */
	DEBUG_EVT_EMULATOR_NETWORK_CONNECTED,

	/** An irrecoverable error has occurred in the debug module. Error details are
	 *  attached in the event structure.
	 */
	DEBUG_EVT_ERROR
};

/** @brief Debug event. */
struct debug_module_event {
	struct app_event_header header;
	enum debug_module_event_type type;

	union {
		/** Code signifying the cause of error. */
		int err;
	} data;
};

APP_EVENT_TYPE_DECLARE(debug_module_event);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* _DEBUG_MODULE_EVENT_H_ */
