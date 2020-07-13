/*
 * Copyright (C) 2007 IBM Corp.
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

#include "spu-tools.h"

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <assert.h>

/***********************************************
 * PER-PROCESS INFORMATION
 ***********************************************/

struct field proc_fields[] = {
	{ PROC_PPU_PID,     "PID",       "%6s",     "%6d",     "PPU Side Process ID",       1, "PPU_PID" },
	{ PROC_N_THREADS,   "N",         "%3s",     "%3d",     "Number of SPE Threads",     1, "N_THREADS" },
	{ PROC_USER,        "USERNAME",  " %-10s",  " %-10s",  "User",                      1, "USER_NAME" },
	{ PROC_PERCENT_SPU, "%SPU",      "%7s",     "%7.1f",   "SPU Usage",                 1, "PERCENT_SPU" },

	{ PROC_TOTAL_TIME,  "TIME",  "%9s",  "%9.3f", "Resident Time (loaded or running) in seconds",    1, "TOTAL_TIME" },
	{ PROC_USER_TIME,   "USER",  "%9s",  "%9.3f", "User Time in seconds",                            0, "USER_TIME" },
	{ PROC_SYSTEM_TIME, "SYS",   "%9s",  "%9.3f", "System Time in seconds",                          0, "SYSTEM_TIME" },
	{ PROC_IOWAIT_TIME, "WAIT",  "%9s",  "%9.3f", "IO/Wait Time in seconds",                         0, "IOWAIT_TIME" },
	{ PROC_LOADED_TIME, "LOAD",  "%9s",  "%9.3f", "Loaded Time in seconds",                          0, "LOADED_TIME" },

	{ PROC_VOLUNTARY_CTX_SWITCHES,   "VCSW",    "%5s", "%5llu", "Number of Voluntary Context Switches",     0, "VOLUNTARY_CTX_SWITCHES" },
	{ PROC_INVOLUNTARY_CTX_SWITCHES, "ICSW",    "%5s", "%5llu", "Number of Involuntary Context Switches",   0, "INVOLUNTARY_CTX_SWITCHES" },
	{ PROC_SLB_MISSES,               "SLB",     "%5s", "%5llu", "Number of SLB Misses",                     0, "SLB_MISSES"},
	{ PROC_HASH_FAULTS,              "HFLT",    "%5s", "%5llu", "Number of Hash Faults",                    0, "HASH_FAULTS" },
	{ PROC_MINOR_PAGE_FAULTS,        "mFLT",    "%5s", "%5llu", "Number of Minor Page Faults",              0, "MINOR_PAGE_FAULTS" },
	{ PROC_MAJOR_PAGE_FAULTS,        "MFLT",    "%5s", "%5llu", "Number of Major Page Faults",              0, "MAJOR_PAGE_FAULTS" },
	{ PROC_CLASS2_INTERRUPTS,        "IRQ2",    "%5s", "%5llu", "Number of Class2 Interrupts Received",     0, "CLASS2_INTERRUPTS" },
	{ PROC_PPE_LIBRARY,              "PPE_LIB", "%8s", "%8llu", "Number of PPE Assisted Library Performed", 0, "PPE_LIBRARY" },

	{ PROC_BINARY_NAME,              "BINARY", " %-18s",  " %-18s", "Binary Name",               1, "BINARY_NAME" },

	{ 0,            NULL,   NULL ,     NULL,    NULL,                        0, NULL }
};

static int proc_sort_descending;

static enum proc_field_id proc_sort_field = PROC_PPU_PID;

inline enum proc_field_id get_proc_sort_field()
{
	return proc_sort_field;
}

inline void set_proc_sort_field(enum proc_field_id field)
{
	proc_sort_field = field;
}

void set_proc_sort_descending(int descending)
{
	proc_sort_descending  = descending;
}

#define DEFAULT_CAPACITY 32
static struct proc** procs;
static int procs_n;
static int procs_capacity;

static int procs_ensure_capacity(int n)
{
	while (procs_capacity < n) {
		void* ret;
		ret = realloc(procs, sizeof(struct ctx*) *
			(procs_capacity + DEFAULT_CAPACITY));
		if (!ret)
			exit(-ENOMEM);
		procs = ret;
		procs_capacity += DEFAULT_CAPACITY;
	}
	return procs_capacity;
}

static int procs_compare(const void *v1, const void *v2)
{
	int ret;
	const struct proc *p1, *p2;

	p1 = *(struct proc * const *)v1;
	p2 = *(struct proc * const *)v2;

	switch (proc_sort_field) {
		case PROC_PPU_PID:
			ret = p1->ppu_pid - p2->ppu_pid; break;
		case PROC_USER:
			ret = strcmp(p1->user, p2->user); break;
		case PROC_PERCENT_SPU:
			ret = p1->percent_spu - p2->percent_spu; break;
		case PROC_TOTAL_TIME:
			ret = (p1->time[TIME_TOTAL] - p2->time[TIME_TOTAL]); break;

		case PROC_USER_TIME:
			ret = (p1->time[TIME_USER] - p2->time[TIME_USER]); break;
		case PROC_SYSTEM_TIME:
			ret = (p1->time[TIME_SYSTEM] - p2->time[TIME_SYSTEM]); break;
		case PROC_IOWAIT_TIME:
			ret = (p1->time[TIME_IOWAIT] - p2->time[TIME_IOWAIT]); break;
		case PROC_LOADED_TIME:
			ret = (p1->time[TIME_LOADED] - p2->time[TIME_LOADED]); break;

		case PROC_BINARY_NAME:
			ret = strcmp(p1->binary_name, p2->binary_name); break;

		case PROC_VOLUNTARY_CTX_SWITCHES:
			ret = p1->voluntary_ctx_switches - p2->voluntary_ctx_switches; break;
		case PROC_INVOLUNTARY_CTX_SWITCHES:
			ret = p1->involuntary_ctx_switches - p2->involuntary_ctx_switches; break;
		case PROC_SLB_MISSES:
			ret = p1->slb_misses - p2->slb_misses; break;
		case PROC_HASH_FAULTS:
			ret = p1->hash_faults - p2->hash_faults; break;
		case PROC_MINOR_PAGE_FAULTS:
			ret = p1->minor_page_faults - p2->minor_page_faults; break;
		case PROC_MAJOR_PAGE_FAULTS:
			ret = p1->major_page_faults - p2->major_page_faults; break;
		case PROC_CLASS2_INTERRUPTS:
			ret = p1->class2_interrupts - p2->class2_interrupts; break;
		case PROC_PPE_LIBRARY:
			ret = p1->ppe_library - p2->ppe_library; break;

		default:
			ret = 0;
	}

	return proc_sort_descending? -ret : ret;
}

struct proc **get_procs(struct ctx** ctxs)
{
	int i, j, proc_idx;
	enum time time;

	if (!ctxs)
		return NULL;
	for (i = 0; ctxs[i]; i++)
		ctxs[i]->updated = 0;

	for (i = 0, proc_idx = 0; ctxs[i]; i++) {
		if (ctxs[i]->updated)
			continue;

		procs_ensure_capacity(proc_idx + 1);
		if (proc_idx >= procs_n) {
			procs[proc_idx] = malloc(sizeof(struct proc));
			procs_n = proc_idx + 1;
		}
		bzero(procs[proc_idx], sizeof(struct proc));
		procs[proc_idx]->binary_name = ctxs[i]->binary_name;
		procs[proc_idx]->user = ctxs[i]->user;
		procs[proc_idx]->ppu_pid = ctxs[i]->ppu_pid;

		for (j = i; ctxs[j]; j++) {
			if (ctxs[i]->ppu_pid != ctxs[j]->ppu_pid)
				continue;

			for_each_time(time)
				procs[proc_idx]->time[time] += ctxs[j]->time[time];

			procs[proc_idx]->percent_spu += ctxs[j]->percent_spu;

			procs[proc_idx]->voluntary_ctx_switches += ctxs[j]->voluntary_ctx_switches;
			procs[proc_idx]->involuntary_ctx_switches += ctxs[j]->involuntary_ctx_switches;
			procs[proc_idx]->slb_misses += ctxs[j]->slb_misses;
			procs[proc_idx]->hash_faults += ctxs[j]->hash_faults;
			procs[proc_idx]->minor_page_faults += ctxs[j]->minor_page_faults;
			procs[proc_idx]->major_page_faults += ctxs[j]->major_page_faults;
			procs[proc_idx]->class2_interrupts += ctxs[j]->class2_interrupts;
			procs[proc_idx]->ppe_library += ctxs[j]->ppe_library;

			procs[proc_idx]->n_threads++;

			ctxs[j]->updated = 1;
		}
		proc_idx++;
	}

	int old_procs_n = procs_n;
	for (i = proc_idx; i < old_procs_n; i++) {
		assert(procs[i]);
		free(procs[i]);
		procs[i] = NULL;
		procs_n--;
	}
	procs_ensure_capacity(proc_idx + 1);
	procs[proc_idx] = NULL;

	qsort(procs, procs_n, sizeof(struct proc *), procs_compare);

	return procs;
}

int print_proc_field(struct proc *proc, char *buf, enum proc_field_id field,
		     const char *format)
{
	switch(field) {
		case PROC_PPU_PID:
			return sprintf(buf, format, proc->ppu_pid);
		case PROC_N_THREADS:
			return sprintf(buf, format, proc->n_threads);
		case PROC_USER:
			return sprintf(buf, format, proc->user);
		case PROC_PERCENT_SPU:
			return sprintf(buf, format, proc->percent_spu);

		case PROC_TOTAL_TIME:
			return sprintf(buf, format, proc->time[TIME_TOTAL] / 1000.0);
		case PROC_USER_TIME:
			return sprintf(buf, format, proc->time[TIME_USER] / 1000.0);
		case PROC_SYSTEM_TIME:
			return sprintf(buf, format, proc->time[TIME_SYSTEM] / 1000.0);
		case PROC_IOWAIT_TIME:
			return sprintf(buf, format, proc->time[TIME_IOWAIT] / 1000.0);
		case PROC_LOADED_TIME:
			return sprintf(buf, format, proc->time[TIME_LOADED] / 1000.0);

		case PROC_BINARY_NAME:
			return sprintf(buf, format, proc->binary_name);

		case PROC_VOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, proc->voluntary_ctx_switches);
		case PROC_INVOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, proc->involuntary_ctx_switches);
		case PROC_SLB_MISSES:
			return sprintf(buf, format, proc->slb_misses);
		case PROC_HASH_FAULTS:
			return sprintf(buf, format, proc->hash_faults);
		case PROC_MINOR_PAGE_FAULTS:
			return sprintf(buf, format, proc->minor_page_faults);
		case PROC_MAJOR_PAGE_FAULTS:
			return sprintf(buf, format, proc->major_page_faults);
		case PROC_CLASS2_INTERRUPTS:
			return sprintf(buf, format, proc->class2_interrupts);
		case PROC_PPE_LIBRARY:
			return sprintf(buf, format, proc->ppe_library);
		default :
			return 0;
	}
}
