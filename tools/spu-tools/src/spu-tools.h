/*
 * Copyright (C) 2006 IBM Corp.
 *
 * Author: Andre Detsch <adetsch@br.ibm.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef SPUPS_H
#define SPUPS_H

#include <dirent.h>

#ifndef SPUFS_PATH
#define SPUFS_PATH  "/spu"
#endif

#ifndef PROCFS_PATH
#define PROCFS_PATH "/proc"
#endif

#ifndef SYSFS_PATH
#define SYSFS_PATH "/sys"
#endif

typedef unsigned long long u64;
typedef unsigned int u32;
typedef char s8;

struct field {
	char id;
	const char* name;
	const char* name_format;

	const char *format;
	const char *description;
	int do_show;
	const char *id_string;
};


enum time { TIME_USER=0, TIME_NICE, TIME_SYSTEM, TIME_IOWAIT, TIME_LOADED,
		TIME_TOTAL, TIME_IDLE, TIME_MAX };

#define for_each_time(time) for((time)=0; time<TIME_MAX; time++)
float PERCENT(u64 old_t, u64 new_t, u64 tot_t);

/*
 * PER_CTX
 */

#define SPE_UNKNOWN  -1
#define SPE_NONE     -2

enum ctx_field_id {
	CTX_INVALID=0,
	CTX_PPU_PID='a',
	CTX_THREAD_ID,
	CTX_USER,
	CTX_STATUS,
	CTX_FLAGS,
	CTX_PERCENT_SPU,
	CTX_SPE,
	CTX_TOTAL_TIME,
	CTX_USER_TIME,
	CTX_SYSTEM_TIME,
	CTX_LOADED_TIME,
	CTX_VOLUNTARY_CTX_SWITCHES,
	CTX_INVOLUNTARY_CTX_SWITCHES,
	CTX_SLB_MISSES,
	CTX_HASH_FAULTS,
	CTX_MINOR_PAGE_FAULTS,
	CTX_MAJOR_PAGE_FAULTS,
	CTX_CLASS2_INTERRUPTS,
	CTX_PPE_LIBRARY,
	CTX_BINARY_NAME,
	CTX_MAX_FIELD
};

struct ctx {
	u64         context_id;
	int         ppu_pid;
	int         thread_id;
	const char *user;
	char        status;
	char        flags;       /* Isolated / Normal */
	int         spe;
	char       *binary_name;

	u64 time[TIME_MAX];

	float percent_spu;

	u64 voluntary_ctx_switches;
	u64 involuntary_ctx_switches;
	u64 slb_misses;
	u64 hash_faults;
	u64 minor_page_faults;
	u64 major_page_faults;
	u64 class2_interrupts;
	u64 ppe_library;

	int updated;
};

extern struct field ctx_fields[];

struct ctx **get_spu_contexts(u64 period);

int print_ctx_field(struct ctx *ctx, char *buf, enum ctx_field_id field,
		     const char *format);

inline void set_ctx_sort_field(enum ctx_field_id field);
inline enum ctx_field_id get_ctx_sort_field();
void set_ctx_sort_descending(int descending);


/*
 * PER_SPU
 */

enum spu_field_id {
	SPU_INVALID=0,
	SPU_NUMBER='a',
	SPU_STATE,
	SPU_PERCENT_SPU,
	SPU_PERCENT_USER,
	SPU_PERCENT_SYSTEM,
	SPU_PERCENT_IOWAIT,
	SPU_VOLUNTARY_CTX_SWITCHES,
	SPU_INVOLUNTARY_CTX_SWITCHES,
	SPU_SLB_MISSES,
	SPU_HASH_FAULTS,
	SPU_MINOR_PAGE_FAULTS,
	SPU_MAJOR_PAGE_FAULTS,
	SPU_CLASS2_INTERRUPTS,
	SPU_PPE_LIBRARY,
	SPU_MAX_FIELD
};

struct spu {
	int number;

	char state;

	u64 time[TIME_MAX];
	u64 last_time[TIME_MAX];

	u64 voluntary_ctx_switches;
	u64 involuntary_ctx_switches;
	u64 slb_misses;
	u64 hash_faults;
	u64 minor_page_faults;
	u64 major_page_faults;
	u64 class2_interrupts;
	u64 ppe_library;

	float percent[TIME_MAX];
};


extern struct field spu_fields[];

struct spu **get_spus();

int print_spu_field(struct spu *spu, char *buf, enum spu_field_id field,
		     const char *format);

inline enum spu_field_id get_spu_sort_field();
inline void set_spu_sort_field(enum spu_field_id field);
void set_spu_sort_descending(int descending);


/*
 * PER_PROC
 */

enum proc_field_id {
	PROC_INVALID=0,
	PROC_PPU_PID='a',
	PROC_N_THREADS,
	PROC_USER,
	PROC_PERCENT_SPU,
	PROC_TOTAL_TIME,
	PROC_USER_TIME,
	PROC_SYSTEM_TIME,
	PROC_IOWAIT_TIME,
	PROC_LOADED_TIME,
	PROC_VOLUNTARY_CTX_SWITCHES,
	PROC_INVOLUNTARY_CTX_SWITCHES,
	PROC_SLB_MISSES,
	PROC_HASH_FAULTS,
	PROC_MINOR_PAGE_FAULTS,
	PROC_MAJOR_PAGE_FAULTS,
	PROC_CLASS2_INTERRUPTS,
	PROC_PPE_LIBRARY,
	PROC_BINARY_NAME,
	PROC_MAX_FIELD
};

struct proc {
	int         ppu_pid;
	const char *user;
	char       *binary_name;

	u64 time[TIME_MAX];

	float percent_spu;

	u64 voluntary_ctx_switches;
	u64 involuntary_ctx_switches;
	u64 slb_misses;
	u64 hash_faults;
	u64 minor_page_faults;
	u64 major_page_faults;
	u64 class2_interrupts;
	u64 ppe_library;

	int n_threads;
};

extern struct field proc_fields[];

struct proc **get_procs(struct ctx** ctxs);

int print_proc_field(struct proc *proc, char *buf, enum proc_field_id field,
		     const char *format);

inline enum proc_field_id get_proc_sort_field();
inline void set_proc_sort_field(enum proc_field_id field);
void set_proc_sort_descending(int descending);

/*
 * GENERAL
 */

int count_cpus();
int count_spus();

void get_spus_loadavg(float *avg1min, float *avg5min, float *avg15min);
void get_cpus_loadavg(float *avg1min, float *avg5min, float *avg15min);

void get_spu_stats(struct spu** spus, float *percents);

void get_cpu_stats(float *percents);


#endif

