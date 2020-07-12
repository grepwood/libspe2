/*
 * libspe2 - A wrapper to allow direct execution of SPE binaries
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 *  License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this library; if not, write to the Free Software Foundation,
 *   Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef libspe2_types_h
#define libspe2_types_h

#ifdef __cplusplus
extern "C"
{
#endif

#include <limits.h>
#include "cbea_map.h"

/*
 * Data structure declarations for
 * SPE problem state areas
 * (part of public API)
 */

/** SPE program handle
 * Structure spe_program_handle per CESOF specification
 * libspe2 applications usually only keep a pointer
 * to the program handle and do not use the structure
 * directly.
 */
typedef struct spe_program_handle {
	/*
	 * handle_size allows for future extensions of the spe_program_handle 
	 * struct by new fields, without breaking compatibility with existing users. 
	 * Users of the new field would check whether the size is large enough. 
	 */
	unsigned int	handle_size;
	void		*elf_image;
	void		*toe_shadow;
} spe_program_handle_t;


/** SPE context
 * The SPE context is one of the base data structures for
 * the libspe2 implementation. It holds all persistent information
 * about a "logical SPE" used by the application. This data
 * structure should not be accessed directly, but the application
 * uses a pointer to an SPE context as an identifier for the
 * "logical SPE" it is dealing with through libspe2 API calls.
 */

struct spe_context 
 {
	/*
	 * Note: The debugger accesses this data structure, and assumes 
	 * it starts out with an spe_program_handle_t identifying the SPE
	 * executable running within this SPE thread.  Everything below
	 * is private to libspe.  
	 */
	spe_program_handle_t handle;
	/*  
	 * PRIVATE TO IMPLEMENTATION - DO NOT USE DIRECTLY
	 */
	struct spe_context_base_priv * base_private;
	struct spe_context_event_priv * event_private;
};
/** spe_context_ptr_t
 * 	This pointer serves as the identifier for a specific
 *	SPE context throughout the API (where needed)
 */
typedef struct spe_context * spe_context_ptr_t;

/** SPE gang context
 * The SPE gang context is one of the base data structures for
 * the libspe2 implementation. It holds all persistent information
 * about a group of SPE contexts that should be treated as a gang,
 * i.e., be execute together with certain properties. This data
 * structure should not be accessed directly, but the application
 * uses a pointer to an SPE gang context as an identifier for the
 * SPE gang it is dealing with through libspe2 API calls. 
 */
struct spe_gang_context
{
	/*  
	 * PRIVATE TO IMPLEMENTATION - DO NOT USE DIRECTLY
	 */
	struct spe_gang_context_base_priv * base_private;
	struct spe_gang_context_event_priv * event_private;
};
/** spe_gang_context_ptr_t
 * 	This pointer serves as the identifier for a specific
 *	SPE gang context throughout the API (where needed)
 */
typedef struct spe_gang_context * spe_gang_context_ptr_t;

/*
 * SPE stop information
 * This structure is used to return all information available 
 * on the reason why an SPE program stopped execution. 
 * This information is important for some advanced programming 
 * patterns and/or detailed error reporting.
 */
 
/** spe_stop_info_t
 */
typedef struct spe_stop_info {
	unsigned int stop_reason;
	union {
		int spe_exit_code;
		int spe_signal_code;
		int spe_runtime_error;
		int spe_runtime_exception; 
		int spe_runtime_fatal;
		int spe_callback_error;
		int spe_isolation_error;
		/* Reserved fields */
		void *__reserved_ptr;
		unsigned long long __reserved_u64;
	} result;
	int spu_status;
} spe_stop_info_t;

/*
 * SPE event structure
 * This structure is used for SPE event handling
 */
 
/** spe_event_data_t
 * User data to be associated with an event
 */
typedef union spe_event_data
{
  void *ptr;
  unsigned int u32;
  unsigned long long u64;
} spe_event_data_t;

/** spe_event_t
 */
typedef struct spe_event_unit
{
  unsigned int events;
  spe_context_ptr_t spe;
  spe_event_data_t data;
} spe_event_unit_t;

typedef void* spe_event_handler_ptr_t;
typedef int spe_event_handler_t;


/*
 *---------------------------------------------------------
 * 
 * API symbolic constants
 * 
 *---------------------------------------------------------
 */

enum ps_area { SPE_MSSYNC_AREA, SPE_MFC_COMMAND_AREA, SPE_CONTROL_AREA, SPE_SIG_NOTIFY_1_AREA, SPE_SIG_NOTIFY_2_AREA };

/** 
 * Flags for spe_context_create
 */
#define SPE_CFG_SIGNOTIFY1_OR   0x00000010
#define SPE_CFG_SIGNOTIFY2_OR   0x00000020
#define SPE_MAP_PS				0x00000040
#define SPE_ISOLATE				0x00000080
#define SPE_ISOLATE_EMULATE			0x00000100
#define SPE_EVENTS_ENABLE			0x00001000
#define SPE_AFFINITY_MEMORY			0x00002000


/**
 * Symbolic constants for stop reasons
 * as returned in spe_stop_info_t
 */
#define SPE_EXIT				1
#define SPE_STOP_AND_SIGNAL		2
#define SPE_RUNTIME_ERROR		3
#define SPE_RUNTIME_EXCEPTION	4
#define SPE_RUNTIME_FATAL		5
#define SPE_CALLBACK_ERROR		6
#define SPE_ISOLATION_ERROR		7

/**
 * Runtime errors
 */
#define SPE_SPU_STOPPED_BY_STOP     0x02 /* INTERNAL USE ONLY */
#define SPE_SPU_HALT                0x04
#define SPE_SPU_WAITING_ON_CHANNEL  0x08 /* INTERNAL USE ONLY */
#define SPE_SPU_SINGLE_STEP         0x10
#define SPE_SPU_INVALID_INSTR       0x20
#define SPE_SPU_INVALID_CHANNEL     0x40

/**
 * Runtime exceptions
 */
#define SPE_DMA_ALIGNMENT               0x0008
/* #define SPE_SPE_ERROR                   0x0010 */
#define SPE_DMA_SEGMENTATION            0x0020
#define SPE_DMA_STORAGE                 0x0040
/* #define SPE_INVALID_DMA                 0x0800 */

/**
 * SIGSPE maps to SIGURG 
 */
#define SIGSPE SIGURG

/**
 * Supported SPE events
 */
#define SPE_EVENT_OUT_INTR_MBOX		0x00000001
#define SPE_EVENT_IN_MBOX		0x00000002
#define SPE_EVENT_TAG_GROUP		0x00000004
#define SPE_EVENT_SPE_STOPPED		0x00000008

#define SPE_EVENT_ALL_EVENTS		SPE_EVENT_OUT_INTR_MBOX | \
					SPE_EVENT_IN_MBOX | \
					SPE_EVENT_TAG_GROUP | \
					SPE_EVENT_SPE_STOPPED

/**
 * Behavior flags for mailbox read/write functions
 */
#define SPE_MBOX_ALL_BLOCKING		1
#define SPE_MBOX_ANY_BLOCKING		2
#define SPE_MBOX_ANY_NONBLOCKING	3


/**
 * Behavior flags tag status functions
 */
#define SPE_TAG_ALL			1
#define SPE_TAG_ANY			2
#define SPE_TAG_IMMEDIATE		3


/**
 * Flags for _base_spe_context_run
 */
#define SPE_DEFAULT_ENTRY   UINT_MAX
#define SPE_RUN_USER_REGS	0x00000001	/* 128b user data for r3-5.   */
#define SPE_NO_CALLBACKS	0x00000002

/*
 *
 */
#define SPE_CALLBACK_NEW             1 
#define SPE_CALLBACK_UPDATE          2



#define SPE_COUNT_PHYSICAL_CPU_NODES 1
#define SPE_COUNT_PHYSICAL_SPES      2
#define SPE_COUNT_USABLE_SPES        3

/**
 * Signal Targets 
 */
#define SPE_SIG_NOTIFY_REG_1		0x0001
#define SPE_SIG_NOTIFY_REG_2		0x0002

#ifdef __cplusplus
}
#endif

#endif /*libspe2_types_h*/
