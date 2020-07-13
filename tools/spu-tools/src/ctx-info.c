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
#include <ctype.h>

float PERCENT(u64 old_t, u64 new_t, u64 tot_t) {
	float ret = ((tot_t) > 0)?
		((new_t) - (old_t)) * 100.0 / (tot_t) :
		0.0;
	return ret <= 100.0? ret : 100.0;
}

static int ctx_sort_descending;
static enum ctx_field_id ctx_sort_field = CTX_PPU_PID;

void set_ctx_sort_descending(int descending)
{
	ctx_sort_descending = descending;
}

inline void set_ctx_sort_field(enum ctx_field_id field)
{
	ctx_sort_field = field;
}

inline enum ctx_field_id get_ctx_sort_field()
{
	return ctx_sort_field;
}

/**********************************************
 * PER-CTX INFORMATION
 **********************************************/

#define DEFAULT_CAPACITY 32
static struct ctx** ctxs;
static int ctxs_n;
static int ctxs_capacity;


/* USERNAME CACHE */

struct uid_name {
	const char *name;
	int uid;
	struct uid_name *next;
};

static struct uid_name *uid_name_head;

static void add_to_cache(int uid, const char *name)
{
	struct uid_name *un;

	un = malloc(sizeof(*un));
	un->uid = uid;
	un->name = name;
	un->next = uid_name_head;
	uid_name_head = un;
}

static const char *lookup_uid_cache(int uid)
{
	struct uid_name *un;

	for (un = uid_name_head; un; un = un->next)
		if (un->uid == uid)
			return un->name;
	return NULL;
}

static const char *get_username(int uid)
{
	const char *name;

	name = lookup_uid_cache(uid);
	if (!name) {
		struct passwd *user_data = getpwuid(uid);
		if (user_data)
			name = strdup(user_data->pw_name);
		else
			name = "UNKNOWN";
		add_to_cache(uid, name);
	}
	return name;
}


struct field ctx_fields[] = {
	{ CTX_PPU_PID,     "PID",       "%6s",     "%6d",     "PPU Side Process ID",       1, "PPU_PID" },
	{ CTX_THREAD_ID,   "TID",       "%6s",     "%6d",     "SPE Controlling Thread ID", 1, "THREAD_ID" },
	{ CTX_USER,        "USERNAME",  " %-10s",  " %-10s",  "User",                      1, "USER_NAME" },
	{ CTX_STATUS,      "S",         "%2s",     "%2c",     "Context Status",            1, "STATUS" },
	{ CTX_FLAGS,       "F",         "%2s",     "%2c",     "Context Flags",             1, "FLAGS" },
	{ CTX_PERCENT_SPU, "%SPU",      "%6s",     "%6.1f",   "SPU Usage",                 1, NULL },
	{ CTX_SPE,         "SPE",       "%4s",     "%4d",     "Allocated SPE",             1, "SPE" },
	
	{ CTX_TOTAL_TIME,  "TIME",      "%9s",     "%9.3f",   "Resident Time (loaded or running) in seconds",  1, "TOTAL_TIME" },
	{ CTX_USER_TIME,   "USER",      "%9s",     "%9.3f",   "User Time in seconds",                          0, "USER_TIME" },
	{ CTX_SYSTEM_TIME, "SYS",       "%9s",     "%9.3f",   "System Time in seconds",                        0, "SYSTEM_TIME" },
	{ CTX_LOADED_TIME, "LOAD",      "%9s",     "%9.3f",   "Loaded Time (loaded or running) in seconds",    0, "LOADED_TIME" },

	{ CTX_VOLUNTARY_CTX_SWITCHES,   "VCSW",    "%5s", "%5llu", "Number of Voluntary Context Switches",     0, "VOLUNTARY_CTX_SWITCHES" },
	{ CTX_INVOLUNTARY_CTX_SWITCHES, "ICSW",    "%5s", "%5llu", "Number of Involuntary Context Switches",   0, "INVOLUNTARY_CTX_SWITCHES" },
	{ CTX_SLB_MISSES,               "SLB",     "%5s", "%5llu", "Number of SLB Misses",                     0, "SLB_MISSES"},
	{ CTX_HASH_FAULTS,              "HFLT",    "%5s", "%5llu", "Number of Hash Faults",                    0, "HASH_FAULTS" },
	{ CTX_MINOR_PAGE_FAULTS,        "mFLT",    "%5s", "%5llu", "Number of Minor Page Faults",              0, "MINOR_PAGE_FAULTS" },
	{ CTX_MAJOR_PAGE_FAULTS,        "MFLT",    "%5s", "%5llu", "Number of Major Page Faults",              0, "MAJOR_PAGE_FAULTS" },
	{ CTX_CLASS2_INTERRUPTS,        "IRQ2",    "%5s", "%5llu", "Number of Class2 Interrupts Received",     0, "CLASS2_INTERRUPTS" },
	{ CTX_PPE_LIBRARY,              "PPE_LIB", "%8s", "%8llu", "Number of PPE Assisted Library Calls Performed", 0, "PPE_LIBRARY" },

	{ CTX_BINARY_NAME, "BINARY", " %-18s",  " %-18s", "Binary Name",               1, "BINARY_NAME" },

	{ 0,            NULL,   NULL ,     NULL,    NULL,                        0, NULL }
};

int print_ctx_field(struct ctx *ctx, char *buf, enum ctx_field_id field, const char *format)
{
	switch(field) {
		case CTX_PPU_PID:
			return sprintf(buf, format, ctx->ppu_pid);
		case CTX_THREAD_ID:
			return sprintf(buf, format, ctx->thread_id);
		case CTX_USER:
			return sprintf(buf, format, ctx->user);
		case CTX_STATUS:
			return sprintf(buf, format, ctx->status);
		case CTX_FLAGS:
			return sprintf(buf, format, ctx->flags);
		case CTX_PERCENT_SPU:
			return sprintf(buf, format, ctx->percent_spu);
		case CTX_SPE:
			return sprintf(buf, format, ctx->spe);
		case CTX_TOTAL_TIME:
			return sprintf(buf, format, ctx->time[TIME_TOTAL] / 1000.0);
		case CTX_USER_TIME:
			return sprintf(buf, format, ctx->time[TIME_USER] / 1000.0);
		case CTX_SYSTEM_TIME:
			return sprintf(buf, format, ctx->time[TIME_SYSTEM] / 1000.0);
		case CTX_LOADED_TIME:
			return sprintf(buf, format, ctx->time[TIME_LOADED] / 1000.0);
		case CTX_BINARY_NAME:
			return sprintf(buf, format, ctx->binary_name);

		case CTX_VOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, ctx->voluntary_ctx_switches);
		case CTX_INVOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, ctx->involuntary_ctx_switches);
		case CTX_SLB_MISSES:
			return sprintf(buf, format, ctx->slb_misses);
		case CTX_HASH_FAULTS:
			return sprintf(buf, format, ctx->hash_faults);
		case CTX_MINOR_PAGE_FAULTS:
			return sprintf(buf, format, ctx->minor_page_faults);
		case CTX_MAJOR_PAGE_FAULTS:
			return sprintf(buf, format, ctx->major_page_faults);
		case CTX_CLASS2_INTERRUPTS:
			return sprintf(buf, format, ctx->class2_interrupts);
		case CTX_PPE_LIBRARY:
			return sprintf(buf, format, ctx->ppe_library);
		default :
			return 0;
	}
}

static int ctxs_compare(const void *v1, const void *v2)
{
	int ret;
	const struct ctx *p1, *p2;

	p1 = *(struct ctx * const *)v1;
	p2 = *(struct ctx * const *)v2;

	switch (ctx_sort_field) {
		case CTX_PPU_PID:
			ret = p1->ppu_pid - p2->ppu_pid; break;
		case CTX_THREAD_ID:
			ret = p1->thread_id - p2->thread_id; break;
		case CTX_USER:
			ret = strcmp(p1->user, p2->user); break;
		case CTX_STATUS:
			ret = p1->status - p2->status; break;
		case CTX_FLAGS:
			ret = p1->flags - p2->flags; break;
		case CTX_PERCENT_SPU:
			ret = p1->percent_spu - p2->percent_spu; break;
		case CTX_SPE:
			ret = p1->spe - p2->spe; break;
		case CTX_TOTAL_TIME:
			ret = p1->time[TIME_TOTAL] - p2->time[TIME_TOTAL]; break;

		case CTX_USER_TIME:
			ret = p1->time[TIME_USER] - p2->time[TIME_USER]; break;
		case CTX_SYSTEM_TIME:
			ret = p1->time[TIME_SYSTEM] - p2->time[TIME_SYSTEM]; break;
		case CTX_LOADED_TIME:
			ret = p1->time[TIME_LOADED] - p2->time[TIME_LOADED]; break;

		case CTX_BINARY_NAME:
			ret = strcmp(p1->binary_name, p2->binary_name); break;

		case CTX_VOLUNTARY_CTX_SWITCHES:
			ret = p1->voluntary_ctx_switches - p2->voluntary_ctx_switches; break;
		case CTX_INVOLUNTARY_CTX_SWITCHES:
			ret = p1->involuntary_ctx_switches - p2->involuntary_ctx_switches; break;
		case CTX_SLB_MISSES:
			ret = p1->slb_misses - p2->slb_misses; break;
		case CTX_HASH_FAULTS:
			ret = p1->hash_faults - p2->hash_faults; break;
		case CTX_MINOR_PAGE_FAULTS:
			ret = p1->minor_page_faults - p2->minor_page_faults; break;
		case CTX_MAJOR_PAGE_FAULTS:
			ret = p1->major_page_faults - p2->major_page_faults; break;
		case CTX_CLASS2_INTERRUPTS:
			ret = p1->class2_interrupts - p2->class2_interrupts; break;
		case CTX_PPE_LIBRARY:
			ret = p1->ppe_library - p2->ppe_library; break;

		default:
			ret = 0;
	}

	return ctx_sort_descending? -ret : ret;
}

static struct ctx *alloc_ctx()
{
	struct ctx *s;

	s = (struct ctx *)calloc(1, sizeof(struct ctx));
	s->ppu_pid = 0;
	s->status = '?';
	return s;
}

static int ctxs_ensure_capacity(int n)
{
	while (ctxs_capacity < n) {
		void* ret;
		ret = realloc(ctxs, sizeof(struct ctx*) *
			(ctxs_capacity + DEFAULT_CAPACITY));
		if (!ret)
			exit(-ENOMEM);
		ctxs = ret;
		ctxs_capacity += DEFAULT_CAPACITY;
	}
	return ctxs_capacity;
}


static struct ctx *ctxs_get_ctx(int thread_id)
{
	int i;

	/* This could be optimized by using a hash table */
	for (i = 0; i < ctxs_n; i++) {
		if (ctxs[i]->thread_id == thread_id)
			return ctxs[i];
	}
	return NULL;
}

static int find_thread_pid(int thread_id)
{
	DIR* proc_dir;
	struct dirent *entry;
	static char buf[PATH_MAX];
	int pid = -1;

	proc_dir = opendir(PROCFS_PATH);
	if (proc_dir) {
		while ((entry = readdir(proc_dir)) != NULL) {
			sprintf(buf, "%s/%s/task/%d", PROCFS_PATH, entry->d_name, thread_id);
			if (access(buf, F_OK) == 0) {
				sscanf(entry->d_name, "%d", &pid);
				break;
			}
		}
		closedir(proc_dir);
	}
	return pid;
}

static int
process_ctx_entry(struct dirent *entry, const char *gang_name, u64 last_period)
{
	static char buf[PATH_MAX];
	FILE *fp;
	int uid, thread_id;
	struct ctx *ctx;

	if (gang_name)
		sprintf(buf, "%s/%s/%s", SPUFS_PATH, gang_name, entry->d_name);
	else
		sprintf(buf, "%s/%s", SPUFS_PATH, entry->d_name);
	chdir(buf);

	fp = fopen("tid", "r");
	if (fp) {
		fscanf(fp, "%d", &thread_id);
		fclose(fp);
	} else {
		return 0;
	}

	ctx = ctxs_get_ctx(thread_id);
	if (!ctx) {
		ctxs_ensure_capacity(ctxs_n + 1);
		ctx = alloc_ctx();
		ctxs[ctxs_n] = ctx;
		ctxs_n++;

		ctx->thread_id = thread_id;
		ctx->ppu_pid = find_thread_pid(thread_id);
	}

	sprintf(buf, "%s/%d/stat", PROCFS_PATH, ctx->ppu_pid);
	fp = fopen(buf, "r");
	if (fp) {
		fscanf(fp, "%d %s", &ctx->ppu_pid, buf);
		buf[strlen(buf)-1] = '\0';         /* Remove trailing ')'*/
		ctx->binary_name = strdup(buf+1);  /* Skip initial '('   */
		fclose(fp);
	} else {
		return 0;
	}

	sprintf(buf, "%s/%d/status", PROCFS_PATH, ctx->ppu_pid);
	fp = fopen(buf, "r");
	if (fp) {
		while (fscanf(fp, "%s", buf) != EOF) {
			if (!strncmp(buf, "Uid:", 4)) {
				fscanf(fp, "%d", &uid);
				ctx->user = get_username(uid);
				break;
			}
		}
		fclose(fp);
	}

	if (access("phys-id", R_OK) != 0) {
		ctx->spe = SPE_UNKNOWN;
	} else {
		fp = fopen("phys-id", "r");
		if (fp) {
			fscanf(fp, "%x", &ctx->spe);
			fclose(fp);
		}
	}

	fp = fopen("stat", "r");
	if (fp) {
		u64 last_time;
		last_time = ctx->time[TIME_TOTAL];
		fscanf(fp, "%s %llu %llu %llu %llu "
			"%llu %llu %llu %llu %llu %llu %llu %llu",
			buf,                            /* current SPE state */
			&ctx->time[TIME_USER],          /* total user time in milliseconds */
			&ctx->time[TIME_SYSTEM],        /* total system time in milliseconds */
			&ctx->time[TIME_IOWAIT],        /* total iowait time in milliseconds */
			&ctx->time[TIME_LOADED],        /* total loaded time in milliseconds */
			&ctx->voluntary_ctx_switches,   /* number of voluntary context switches */
			&ctx->involuntary_ctx_switches, /* number of involuntary context switches */
			&ctx->slb_misses,               /* number of SLB misses */
			&ctx->hash_faults,              /* number of hash faults */
			&ctx->minor_page_faults,        /* number of minor page faults */
			&ctx->major_page_faults,        /* number of major page faults */
			&ctx->class2_interrupts,        /* number of class2 interrupts received */
			&ctx->ppe_library);             /* number of ppe assisted library performed */

		fclose(fp);

		ctx->status = toupper(buf[0]);
		ctx->time[TIME_TOTAL] = ctx->time[TIME_USER] + ctx->time[TIME_SYSTEM]
					+ ctx->time[TIME_IOWAIT];
		ctx->percent_spu = PERCENT(last_time, ctx->time[TIME_TOTAL], last_period);
	} else {
		ctx->status = '?';
	}

	fp = fopen("capabilities", "r");
	if (fp) {
		ctx->flags = 'I';
		while (fscanf(fp, "%s", buf) != EOF) {
			if (!strcmp(buf, "sched")) {
				ctx->flags = ' ';
				break;
			}
		}
		fclose(fp);
	}
	else {
		ctx->flags = '?';
	}

	ctx->updated = 1;
	chdir(SPUFS_PATH);
	return 0;
}

void ctxs_delete(int index)
{
	int i;

	assert(ctxs[index]);
	free(ctxs[index]->binary_name);
	for (i = index; i < ctxs_n; i++)
		ctxs[i] = ctxs[i + 1];
	ctxs_n--;
}

struct ctx **get_spu_contexts(u64 last_period)
{
	struct dirent *entry, *gang_entry;
	DIR *ctxs_dir, *gang_dir;
	int i;
	static char buf[PATH_MAX];

	ctxs_dir = opendir(SPUFS_PATH);
	if (!ctxs_dir)
		return NULL;

	for (i = 0; i < ctxs_n; i++)
		ctxs[i]->updated = 0;

	while ((entry = readdir(ctxs_dir)) != NULL) {
		sprintf(buf, "%s/%s/stat", SPUFS_PATH, entry->d_name);
		if (access(buf, F_OK) == 0) {
			process_ctx_entry(entry, NULL, last_period);
		} else {
			/* contexts within a gang */
			sprintf(buf, "%s/%s", SPUFS_PATH, entry->d_name);
			gang_dir = opendir(buf);
			if (gang_dir) {
				while ((gang_entry = readdir(gang_dir)) != NULL) {
					sprintf(buf, "%s/%s/%s/stat", SPUFS_PATH, entry->d_name, gang_entry->d_name);
					if (access(buf, F_OK) == 0)
						process_ctx_entry(gang_entry, entry->d_name, last_period);
				}
				closedir(gang_dir);
			}
		}
	}

	closedir(ctxs_dir);
	for (i = 0; i < ctxs_n; i++) {
		if (!ctxs[i]->updated)
			ctxs_delete(i);
	}

	ctxs_ensure_capacity(ctxs_n + 1);
	ctxs[ctxs_n] = NULL;

	qsort(ctxs, ctxs_n, sizeof(struct ctx *), ctxs_compare);

	return ctxs;
}

