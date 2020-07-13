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

static void print_header(struct field *fields)
{
	int i = 0;
	while (fields[i].id) {
		if (fields[i].do_show)
			printf(fields[i].name_format, fields[i].name);
		i++;
	}
	printf("\n");
}

#define MAX_LINE_SIZE 1024
static void dump_ctxs_or_spus(void **ctxs_or_spus, struct field *fields)
{
	int i = 0;
	char buf[MAX_LINE_SIZE];

	if (!ctxs_or_spus)
		return;

	while (ctxs_or_spus[i]) {
		int j;
		int chars = 0;
		for (j = 0; fields[j].id; j++) {
			if (!fields[j].do_show)
				continue;
			chars += print_ctx_field((struct ctx *)ctxs_or_spus[i],
					buf+chars, fields[j].id, fields[j].format);
		}
		printf(buf);
		printf("\n");
		i++;
	}
	return;
}

#define DUPLICATE_FIELD -1
#define INVALID_FIELD -2
static int set_field_order(const char* field_id_string, int index)
{
	int i;

	for (i = 0; ctx_fields[i].id; i++) {
		if (ctx_fields[i].id_string && !strcmp(ctx_fields[i].id_string, field_id_string)) {
			if (i < index)
				return DUPLICATE_FIELD;

			struct field tmp;
			tmp = ctx_fields[index];
			ctx_fields[index] = ctx_fields[i];
			ctx_fields[i] = tmp;
			ctx_fields[index].do_show = 1;
			return 0;
		}
	}
	return INVALID_FIELD;
}

static void hide_fields(int index)
{
	int i;

	for (i = index; ctx_fields[i].id; i++)
		ctx_fields[i].do_show = 0;
}

static int set_sort_order(const char *id_string)
{
	int i;

	for (i = 0; ctx_fields[i].id; i++) {
		if (ctx_fields[i].id_string && !strcmp(ctx_fields[i].id_string, id_string)) {
			set_ctx_sort_field(ctx_fields[i].id);
			return 0;
		}
	}
	return INVALID_FIELD;
}

#include <getopt.h>

#define die(...) do { fprintf(stderr, __VA_ARGS__); exit(1); } while (0)

static void dump_field_names()
{
	int i;

	for (i = 0; ctx_fields[i].id; i++)
		if (ctx_fields[i].id_string)
			printf("  %-25s - %s\n", ctx_fields[i].id_string, ctx_fields[i].description);
}

static void dump_default_fields()
{
	int first = 1;
	int i;

	for (i = 0; ctx_fields[i].id; i++) {
		if (ctx_fields[i].id_string) {
			printf("%s%s", first?"  ":",", ctx_fields[i].id_string);
			first = 0;
		}
	}
	printf("\n");
}

static void version()
{
	printf(
		"spu-ps (spu-tools 1.0)\n\n"
		"Copyright (C) IBM 2007.\n"
		"Released under the GNU GPL.\n\n"
	);
}


static void usage()
{
	printf(
		"Usage: spu-ps [OPTIONS]\n"
		"Dump information about the running spu contexts.\n\n"
		"Options:\n"
		"  -f, --fields=FIELD,FIELD,...  list of fields to be dumped\n"
		"  -s, --sort=FIELD              sort the output according to the given field\n"
		"  -h, --help                    display this help and exit\n"
		"  -v, --version                 output version information and exit\n\n"
		"Valid field names are:\n"
	);
	dump_field_names();
	printf("\nDefault shown fields are:\n");
	dump_default_fields();
	printf("\nExamples:\n");
	printf("  spu-ps\n\n");
	printf("  spu-ps -f PPU_PID,USER_NAME -s USER_NAME\n\n");
}

int main(int argc, char **argv)
{
	int c;
	int i, ret;

	static struct option long_options[] = {
		{"sort-by",  1, 0, 's'},
		{"fields",   1, 0, 'f'},
		{"help",     0, 0, 'h'},
		{"version",  0, 0, 'v'},
		{0, 0, 0, 0}
	};
	
	for (i = 0; ctx_fields[i].id; i++) {
		if (!ctx_fields[i].id_string)
			ctx_fields[i].do_show = 0;
	}
	
	while (1) {
		c = getopt_long(argc, argv, "f:s:hv", long_options, NULL);
		if (c == -1)
			break;

		char *field;
		switch(c) {
			case 's':
				ret = set_sort_order(optarg);
				if (ret == INVALID_FIELD)
					die("Invalid field name %s.\n", optarg);
				break;

			case 'f':
				i = 0;
				while ((field = strsep(&optarg, ",")) != NULL) {
					ret = set_field_order(field, i++);
					if (ret == DUPLICATE_FIELD)
						die("Field list must contain only one occurence of each field"
							" (%s was listed more than once)\n", field);
					else if (ret == INVALID_FIELD)
						die("Invalid field name %s.\n", field);
				}
				hide_fields(i);
				break;

			case 'v':
				version();
				exit(0);

			default:
				usage();
				exit(0);
		}
	}

	print_header(ctx_fields);
	dump_ctxs_or_spus((void**)get_spu_contexts(1), ctx_fields);

	return 0;
}
