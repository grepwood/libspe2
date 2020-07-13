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


/**********************************************
 * SORTING CRITERIA
 **********************************************/

static int spu_sort_descending;
static enum spu_field_id spu_sort_field = SPU_NUMBER;

void set_spu_sort_descending(int descending)
{
	spu_sort_descending = descending;
}

inline void set_spu_sort_field(enum spu_field_id field)
{
	spu_sort_field = field;
}

inline enum spu_field_id get_spu_sort_field()
{
	return spu_sort_field;
}


/***********************************************
 * PER-SPU INFORMATION
 ***********************************************/

static struct spu** spus;
static int spus_n;

static int spus_compare(const void *v1, const void *v2)
{
	const struct spu *p1, *p2;
	int ret;

	p1 = *(struct spu * const *)v1;
	p2 = *(struct spu * const *)v2;

	switch (spu_sort_field) {
		case SPU_NUMBER:
			ret = p1->number - p2->number;  break;
		case SPU_STATE:
			ret = p1->state - p2->state; break;

		case SPU_PERCENT_SPU:
			ret = p1->percent[TIME_TOTAL] - p2->percent[TIME_TOTAL]; break;
		case SPU_PERCENT_USER:
			ret = p1->percent[TIME_USER] - p2->percent[TIME_USER]; break;
		case SPU_PERCENT_SYSTEM:
			ret = p1->percent[TIME_SYSTEM] - p2->percent[TIME_SYSTEM]; break;
		case SPU_PERCENT_IOWAIT:
			ret = p1->percent[TIME_IOWAIT] - p2->percent[TIME_IOWAIT]; break;

		case SPU_VOLUNTARY_CTX_SWITCHES:
			ret = p1->voluntary_ctx_switches - p2->voluntary_ctx_switches; break;
		case SPU_INVOLUNTARY_CTX_SWITCHES:
			ret = p1->involuntary_ctx_switches - p2->involuntary_ctx_switches; break;
		case SPU_SLB_MISSES:
			ret = p1->slb_misses - p2->slb_misses; break;
		case SPU_HASH_FAULTS:
			ret = p1->hash_faults - p2->hash_faults; break;
		case SPU_MINOR_PAGE_FAULTS:
			ret = p1->minor_page_faults - p2->minor_page_faults; break;
		case SPU_MAJOR_PAGE_FAULTS:
			ret = p1->major_page_faults - p2->major_page_faults; break;
		case SPU_CLASS2_INTERRUPTS:
			ret = p1->class2_interrupts - p2->class2_interrupts; break;
		case SPU_PPE_LIBRARY:
			ret = p1->ppe_library - p2->ppe_library; break;

		default :
			ret = 0; break;
	}
	return spu_sort_descending? -ret : ret;;
}


static int spu_update(struct spu *spu)
{
	FILE* fd;
	u64 period;
	char buf[PATH_MAX];
	enum time time;

	spu->state = '?';

	sprintf(buf, "%s/devices/system/spu/spu%d/stat", SYSFS_PATH, spu->number);
	fd = fopen(buf, "r");
	if (!fd)
		return -1;

	for_each_time(time)
		spu->last_time[time] = spu->time[time];

	fscanf(fd, "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
		buf,
		&spu->time[TIME_USER],
		&spu->time[TIME_SYSTEM],
		&spu->time[TIME_IOWAIT],
		&spu->time[TIME_IDLE],
		&spu->voluntary_ctx_switches,
		&spu->involuntary_ctx_switches,
		&spu->slb_misses,
		&spu->hash_faults,
		&spu->minor_page_faults,
		&spu->major_page_faults,
		&spu->class2_interrupts,
		&spu->ppe_library);
	fclose(fd);
	spu->state = toupper(buf[0]);

	spu->time[TIME_TOTAL] = spu->time[TIME_USER] +
				spu->time[TIME_SYSTEM] +
				spu->time[TIME_IOWAIT];

	period = (spu->time[TIME_TOTAL] - spu->last_time[TIME_TOTAL]) +
		(spu->time[TIME_IDLE] - spu->last_time[TIME_IDLE]);

	for_each_time(time)
		spu->percent[time] = PERCENT(spu->last_time[time], spu->time[time], period);

	return 0;
}

struct spu **get_spus()
{
	struct dirent *entry;
	int i;
	DIR *spus_dir;

	if (!spus) {
		spus_n = count_spus();
		spus = malloc(sizeof(struct spu*) * (spus_n + 1));
		for (i = 0; i < spus_n; i++)
			spus[i] = calloc(1, sizeof(struct spu));
		spus[spus_n] = NULL;

		i = 0;
		spus_dir = opendir(SYSFS_PATH "/devices/system/spu");
		if (!spus_dir) {
			return spus;
		}

		while ((entry = readdir(spus_dir)) != NULL) {
			if (!strncmp(entry->d_name, "spu", 3)) {
				int ret;
				assert(i < spus_n);
				ret = sscanf(entry->d_name, "spu%d", &spus[i]->number);
				assert(ret >= 1);
				i++;
			}
		}
		closedir(spus_dir);
	}

	assert(spus);

	for (i = 0; i < spus_n; i++) {
		spu_update(spus[i]);
	}

	qsort(spus, spus_n, sizeof(struct spu *), spus_compare);

	return spus;
}

struct field spu_fields[] = {
	{ SPU_NUMBER,         "SPE",   "%3s",  "%3d",    "SPE Number",   1, "" },
	{ SPU_PERCENT_SPU,    "%SPU",  "%6s",  "%6.1f",  "SPU Usage",    1, "" },
	{ SPU_PERCENT_USER,   "%USR",  "%6s",  "%6.1f",  "SPU User",     1, "" },
	{ SPU_PERCENT_SYSTEM, "%SYS",  "%6s",  "%6.1f",  "SPU System",   1, "" },
	{ SPU_PERCENT_IOWAIT, "%WAI",  "%6s",  "%6.1f",  "SPU I/O Wait", 1, "" },
	{ SPU_STATE,          "S",     "%2s",  "%2c",    "State",        1, "" },

	{ SPU_VOLUNTARY_CTX_SWITCHES,   "VCSW",    "%6s", "%6llu", "Number of Voluntary Context Switches",     0, "" },
	{ SPU_INVOLUNTARY_CTX_SWITCHES, "ICSW",    "%6s", "%6llu", "Number of Involuntary Context Switches",   0, "" },
	{ SPU_SLB_MISSES,               "SLB",     "%6s", "%6llu", "Number of SLB Misses",                     1, "" },
	{ SPU_HASH_FAULTS,              "HFLT",    "%6s", "%6llu", "Number of Hash Faults",                    1, "" },
	{ SPU_MINOR_PAGE_FAULTS,        "mFLT",    "%6s", "%6llu", "Number of Minor Page Faults",              1, "" },
	{ SPU_MAJOR_PAGE_FAULTS,        "MFLT",    "%6s", "%6llu", "Number of Major Page Faults",              1, "" },
	{ SPU_CLASS2_INTERRUPTS,        "IRQ2",    "%6s", "%6llu", "Number of Class2 Interrupts Received",     1, "" },
	{ SPU_PPE_LIBRARY,              "PPE_LIB", "%8s", "%8llu", "Number of PPE Assisted Library Calls Performed", 1, "" },

	{ 0,            NULL,    NULL,      NULL,    NULL,                        0 }
};

int print_spu_field(struct spu *spu, char *buf, enum spu_field_id field, const char *format)
{
	switch(field) {
		case SPU_NUMBER:
			return sprintf(buf, format, spu->number);
		case SPU_STATE:
			return sprintf(buf, format, spu->state);
		case SPU_PERCENT_SPU:
			return sprintf(buf, format, spu->percent[TIME_TOTAL]);
		case SPU_PERCENT_USER:
			return sprintf(buf, format, spu->percent[TIME_USER]);
		case SPU_PERCENT_SYSTEM:
			return sprintf(buf, format, spu->percent[TIME_SYSTEM]);
		case SPU_PERCENT_IOWAIT:
			return sprintf(buf, format, spu->percent[TIME_IOWAIT]);

		case SPU_VOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, spu->voluntary_ctx_switches);
		case SPU_INVOLUNTARY_CTX_SWITCHES:
			return sprintf(buf, format, spu->involuntary_ctx_switches);
		case SPU_SLB_MISSES:
			return sprintf(buf, format, spu->slb_misses);
		case SPU_HASH_FAULTS:
			return sprintf(buf, format, spu->hash_faults);
		case SPU_MINOR_PAGE_FAULTS:
			return sprintf(buf, format, spu->minor_page_faults);
		case SPU_MAJOR_PAGE_FAULTS:
			return sprintf(buf, format, spu->major_page_faults);
		case SPU_CLASS2_INTERRUPTS:
			return sprintf(buf, format, spu->class2_interrupts);
		case SPU_PPE_LIBRARY:
			return sprintf(buf, format, spu->ppe_library);
		default:
			return 0;
	}
}


