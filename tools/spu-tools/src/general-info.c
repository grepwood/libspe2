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
 * CPUS / SPUS stats
 ***********************************************/

static u64 last_times[TIME_MAX];

void get_cpu_stats(float *percents)
{
	FILE* fd;
	static u64 times[TIME_MAX];
	enum time time;
	u64 irq_time, softirq_time, steal_time, period;
	int ret;

	fd = fopen(PROCFS_PATH "/stat", "r");
	assert(fd);

	steal_time = 0;
	ret = fscanf(fd, "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
		&times[TIME_USER], &times[TIME_NICE], &times[TIME_SYSTEM], &times[TIME_IDLE],
		&times[TIME_IOWAIT], &irq_time, &softirq_time, &steal_time);
	assert(ret >= 7);
	fclose(fd);
	times[TIME_SYSTEM] += irq_time + softirq_time + steal_time;

	times[TIME_TOTAL] = times[TIME_USER] + times[TIME_NICE] + times[TIME_SYSTEM]
			 + times[TIME_IDLE] + times[TIME_IOWAIT];

	assert(times[TIME_TOTAL] >= last_times[TIME_TOTAL]);
	period = times[TIME_TOTAL] - last_times[TIME_TOTAL];

	for_each_time(time) {
		percents[time] = PERCENT(last_times[time],times[time],period);
		last_times[time] = times[time];
	}
}

void get_spu_stats(struct spu** spus, float *percents)
{
	u64 sum[TIME_MAX];
	int i;
	enum time time;

	if (!spus)
		return;

	for_each_time(time)
		sum[time] = 0;

	for (i = 0; spus[i]; i++) {
		for_each_time(time)
			sum[time] += (spus[i]->time[time] - spus[i]->last_time[time]);
	}

	if (i > 0) {
		for_each_time(time)
			percents[time] = PERCENT(0, sum[time], sum[TIME_TOTAL]+sum[TIME_IDLE]);
	}
}

void get_cpus_loadavg(float *avg1min, float *avg5min, float *avg15min)
{
	FILE* fd;
	int ret;

	fd = fopen(PROCFS_PATH "/loadavg", "r");
	if (fd) {
		ret = fscanf(fd, "%f %f %f", avg1min, avg5min, avg15min);
		fclose(fd);
	}
}

void get_spus_loadavg(float *avg1min, float *avg5min, float *avg15min)
{
	FILE* fd;
	int ret;

	fd = fopen(PROCFS_PATH "/spu_loadavg", "r");
	if (fd) {
		ret = fscanf(fd, "%f %f %f", avg1min, avg5min, avg15min);
		fclose(fd);
		assert(ret >= 3);
	}
}


/**********************************************
 * GENERIC FUNCTIONS
 **********************************************/

int count_cpus()
{
	int cpus = 0;
	DIR *dir;
	struct dirent *entry;

	dir = opendir(SYSFS_PATH "/devices/system/cpu");
	if (!dir)
		return 0;
	while ((entry = readdir(dir)) != NULL)
		if (!strncmp(entry->d_name, "cpu", 3))
			cpus++;
	closedir(dir);
	return cpus;
}

int count_spus()
{
	int spus = 0;
	DIR *dir;
	struct dirent *entry;

	dir = opendir(SYSFS_PATH "/devices/system/spu");
	if (!dir)
		return 0;
	while ((entry = readdir(dir)) != NULL)
		if (!strncmp(entry->d_name, "spu", 3))
			spus++;
	closedir(dir);

	return spus;
}
