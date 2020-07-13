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
#include <curses.h>
#include <errno.h>
#include <ctype.h>
#include <sys/time.h>
#include <getopt.h>
#include <assert.h>

#define BLACK_ON_WHITE 1
#define WHITE_ON_BLACK 2

#define DEFAULT_REFRESH_DELAY 30
#define MAX_LINE_SIZE 1024

static int refresh_delay = DEFAULT_REFRESH_DELAY;
static enum screen_mode { PER_CTX, PER_SPU, PER_PROC } screen_mode = PER_CTX;

static void print_header(struct field *fields)
{
	int char_count = 0;
	int i = 0;
	char buf[MAX_LINE_SIZE];

	mvprintw(0, 0, "spu-top: %s View", screen_mode == PER_CTX? "Context" :
			 screen_mode == PER_SPU? "SPU" : "Process");
	wbkgdset(stdscr, COLOR_PAIR(BLACK_ON_WHITE));
	move(6, 0);
	clrtoeol();

	while (fields[i].id) {
		if (fields[i].do_show) {
			char_count += sprintf(buf+char_count, fields[i].name_format, fields[i].name);
		}
		i++;
	}
	if (char_count)
		mvaddstr(6, 0, buf);

	wbkgdset(stdscr, COLOR_PAIR(WHITE_ON_BLACK));
}

static void dump_fields(void **table, struct field *fields)
{
	int cnt;
	int rows, cols;
	int i;
	char buf[MAX_LINE_SIZE];

	int ofs = 7;
	getmaxyx(stdscr, rows, cols);

	if (!table)
		return;

	for (cnt = ofs; cnt < rows; cnt++) {
		move(cnt, 0);
		clrtoeol();
	}

	cnt = ofs;
	for (i = 0; table[i] && i < (rows - ofs); i++) {
		int j;
		int chars = 0;
		for (j = 0; fields[j].id; j++) {
			if (!fields[j].do_show)
				continue;
			if (screen_mode == PER_CTX) {
				chars += print_ctx_field((struct ctx *)table[i],
						buf+chars, fields[j].id, fields[j].format);
			} else if (screen_mode == PER_SPU) {
				chars += print_spu_field((struct spu *)table[i],
						buf+chars, fields[j].id, fields[j].format);
			} else {
				chars += print_proc_field((struct proc *)table[i],
						buf+chars, fields[j].id, fields[j].format);
			}
		}
		if (chars)
			mvaddstr(cnt, 0, buf);
		cnt++;
	}

	return;
}

/* Clean up and leave */
static void quit()
{
	clear();
	endwin();
	exit(0);
}

/* Initializes ncurses library */
static void init_ncurses()
{
	/* Init screen */
	initscr();
	curs_set(0);
	start_color();
	clear();
	nodelay(stdscr, TRUE);
	cbreak();
	noecho();

	/* Init colors */
	init_pair(BLACK_ON_WHITE, COLOR_BLACK, COLOR_WHITE);
	init_pair(WHITE_ON_BLACK, COLOR_WHITE, COLOR_BLACK);

	attrset(COLOR_PAIR(WHITE_ON_BLACK));
	wbkgdset(stdscr, COLOR_PAIR(WHITE_ON_BLACK));
	clear();

	return;
}


void print_cpu_info(int min_time_has_passed)
{
	static float percents[TIME_TOTAL];
	static float avg1min, avg5min, avg15min;

	wbkgdset(stdscr, COLOR_PAIR(WHITE_ON_BLACK));
	get_cpus_loadavg(&avg1min, &avg5min, &avg15min);
	mvprintw(1, 0, "Cpu(s) load avg: %4.2f, %4.2f, %4.2f\n",
		 	avg1min, avg5min, avg15min);

	if (min_time_has_passed)
		get_cpu_stats(percents);
	mvprintw(3, 0, "Cpu(s):%5.1f%%us,%5.1f%%sys,%5.1f%%wait,%5.1f%%nice,%5.1f%%idle",
			percents[TIME_USER], percents[TIME_SYSTEM], percents[TIME_IOWAIT],
			percents[TIME_NICE], percents[TIME_IDLE]);
}

void print_spu_info(struct spu** spus, int min_time_has_passed)
{
	static float percents[TIME_TOTAL];
	static float avg1min, avg5min, avg15min;

	wbkgdset(stdscr, COLOR_PAIR(WHITE_ON_BLACK));

	get_spus_loadavg(&avg1min, &avg5min, &avg15min);
	mvprintw(2, 0, "Spu(s) load avg: %4.2f, %4.2f, %4.2f\n",
		 	avg1min, avg5min, avg15min);

	if (min_time_has_passed)
		get_spu_stats(spus, percents);
	mvprintw(4, 0, "Spu(s):%5.1f%%us,%5.1f%%sys,%5.1f%%wait,%5.1f%%idle",
		 	percents[TIME_USER], percents[TIME_SYSTEM],
			percents[TIME_IOWAIT], percents[TIME_IDLE]);
}

static void config_sort(struct field *fields)
{
	int i;
	char lc;
	const int ofs = 3;
	char max_field = screen_mode == PER_CTX? CTX_MAX_FIELD :
			 screen_mode == PER_SPU? SPU_MAX_FIELD : PROC_MAX_FIELD;

	for (;;) {
		char sort_field = screen_mode == PER_CTX? get_ctx_sort_field() :
					screen_mode == PER_SPU? get_spu_sort_field() :
					get_proc_sort_field();
		erase();
		mvprintw(1, 0, "Select sort field via field letter, type any other key to return");
		for (i = 0; fields[i].id; i++) {
			mvprintw(i + ofs, 0, "%c %c: %-12s = %s",
				fields[i].id == sort_field? '*':' ',
				fields[i].id,
				fields[i].name,
				fields[i].description);
		}

		nocbreak();
		cbreak();
		nodelay(stdscr, FALSE);
		lc = tolower(getch());
		if (lc < 'a' || lc > max_field)
			break;
		screen_mode == PER_CTX? set_ctx_sort_field(lc) :
			screen_mode == PER_SPU? set_spu_sort_field(lc) : set_proc_sort_field(lc);
	}
	halfdelay(refresh_delay);
}

static void config_show(struct field *fields)
{
	int i;
	char lc;
	int ofs = 3;
	char max_field = screen_mode == PER_CTX? CTX_MAX_FIELD :
			 screen_mode == PER_SPU? SPU_MAX_FIELD : PROC_MAX_FIELD;
	
	for (;;) {
		erase();
		mvprintw(1, 0, "Toggle fields via field letter, type any other key to return");
		for (i = 0; fields[i].id; i++) {
			mvprintw(i + ofs, 0, "%c %c: %-12s = %s",
				fields[i].do_show? '*':' ',
				fields[i].id,
				fields[i].name,
				fields[i].description);
		}

		nocbreak();
		cbreak();
		nodelay(stdscr, FALSE);
		lc = tolower(getch());
		if (lc < 'a' || lc > max_field)
			break;

		for (i = 0; fields[i].id; i++) {
			if (fields[i].id == lc) {
				fields[i].do_show = !fields[i].do_show;
				break;
			}
		}
	}
	halfdelay(refresh_delay);
}

static void config_order(struct field *fields)
{
	int i;
	char c, lc;
	int ofs = 4;
	char max_field = screen_mode == PER_CTX? CTX_MAX_FIELD :
			 screen_mode == PER_SPU? SPU_MAX_FIELD : PROC_MAX_FIELD;

	for (;;) {
		erase();
		mvprintw(1, 0, "Upper case letter moves field left, lower case right");
		mvprintw(2, 0, "Any other key to return");
		for (i = 0; fields[i].id; i++) {
			mvprintw(i + ofs, 0, "%c %c: %-12s = %s",
				fields[i].do_show? '*':' ',
				fields[i].id,
				fields[i].name,
				fields[i].description);
		}

		nocbreak();
		cbreak();
		nodelay(stdscr, FALSE);
		c = getch();
		lc = tolower(c);

		if (lc < 'a' || lc > max_field)
			break;

		for (i = 0; fields[i].id; i++) {
			if (fields[i].id == lc) {
				struct field tmp;
				if (isupper(c) && i > 0) {
					tmp = fields[i-1];
					fields[i-1] = fields[i];
					fields[i] = tmp;
				} else if (islower(c) && fields[i+1].id) {
					tmp = fields[i+1];
					fields[i+1] = fields[i];
					fields[i] = tmp;
				}
				break;
			}
		}
	}
	halfdelay(refresh_delay);
}

static void show_help()
{
	int l = 0;

	erase();
	mvprintw(l++, 0, "Help for Interactive Commands - spu-top");
	l++;
	mvprintw(l++, 0, " %-5s - %s", "f", "add or remove fields/columns");
	mvprintw(l++, 0, " %-5s - %s", "o", "adjust fields/columns order");
	l++;
	mvprintw(l++, 0, " %-5s - %s", "O", "set sorting criteria");
	mvprintw(l++, 0, " %-5s - %s", "A", "sort in ascending order");
	mvprintw(l++, 0, " %-5s - %s", "D", "sort in descending order");
	l++;
	mvprintw(l++, 0, " %-5s - %s", "c", "switch to per-context view");
	mvprintw(l++, 0, " %-5s - %s", "p", "switch to per-process view");
	mvprintw(l++, 0, " %-5s - %s", "s", "switch to per-spu view");
	l++;
	mvprintw(l++, 0, " %-5s - %s", "h,H,?", "displays this help screen");
	mvprintw(l++, 0, " %-5s - %s", "q,Q", "quit program");
	l++;
	mvprintw(l, 0, "Press any key to return.");

	nocbreak();
	cbreak();
	nodelay(stdscr, FALSE);
	getch();
	halfdelay(refresh_delay);
}

static void version()
{
	printf(
		"spu-top (spu-tools 1.0)\n\n"
		"Copyright (C) IBM 2007.\n"
		"Released under the GNU GPL.\n\n"
	);
}

static void usage()
{
	printf(
		"Usage: spu-top [OPTIONS]\n"
		"The spu-top program provides a dynamic real-time view of the\n"
		"running system in regards to Cell/B.E. SPUs. It provides 3 view modes:""\n\n"
		"* Per-Context: information about all instantiated SPU contexts.""\n\n"
		"* Per-Process: same information as in Per-Context view, but consolidating\n"
		"all data from the contexts belonging to a same process in a single line.\n"
		"That means that statistics such as spu usage may reach number_of_spus * 100%%.\n\n"
		"* Per-SPU: information about each physical SPU present on the system.""\n\n"
		"Selection of mode and shown fields can be done within the program.""\n"
		"Press 'h' while running spu-top for help on the interactive commands.""\n\n"
		"Options:\n"
		"  -h, --help                    display this help and exit\n"
		"  -v, --version                 output version information and exit\n\n"
	);
}

int main(int argc, char **argv)
{
	int c;
	u64 period;
	int do_quit = 0;
	struct spu** spus;
	struct proc** procs;
	struct ctx** ctxs;
	struct timeval last_time, current_time;
	char* term;

	/* Parse options */
	static struct option long_options[] = {
		{"help",     0, 0, 'h'},
		{"version",  0, 0, 'v'},
		{0, 0, 0, 0}
	};
	
	while (1) {
		c = getopt_long(argc, argv, "hv", long_options, NULL);

		if (c == -1)
			break;

		switch(c) {
			case 'v':
				version();
				exit(0);

			default:
				usage();
				exit(0);
		}
	}

	/* Init ncurses library */
	init_ncurses();
	halfdelay(refresh_delay);
	keypad(stdscr, true);
	term = getenv("TERM");
	if (!strcmp(term, "xterm") || !strcmp(term, "xterm-color") || !strcmp(term, "vt220")) {
		define_key("\033[H", KEY_HOME);
		define_key("\033[F", KEY_END);
		define_key("\033OP", KEY_F(1));
		define_key("\033OQ", KEY_F(2));
		define_key("\033OR", KEY_F(3));
		define_key("\033OS", KEY_F(4));
		define_key("\033[11~", KEY_F(1));
		define_key("\033[12~", KEY_F(2));
		define_key("\033[13~", KEY_F(3));
		define_key("\033[14~", KEY_F(4));
		define_key("\033[17;2~", KEY_F(18));
	}

	/* Handle signals */
	atexit(quit);
	signal(SIGALRM, quit);
	signal(SIGHUP, quit);
	signal(SIGINT, quit);
	signal(SIGPIPE, quit);
	signal(SIGQUIT, quit);
	signal(SIGTERM, quit);

	last_time.tv_sec = 0;
	last_time.tv_usec = 0;

	/* Providing a valid time range for the first measure (0.055 sec) */
	spus = get_spus();
	ctxs = get_spu_contexts(refresh_delay);
	procs = get_procs(ctxs);
	usleep(55000);

	while (!do_quit) {
		int ch;
		int min_time_has_passed;
		
		erase();
		gettimeofday(&current_time, NULL);

		/* Update data only if time interval > 100ms */
		period = (u64)((double)current_time.tv_sec*1000 + (double)current_time.tv_usec/1000)
			- ((double)last_time.tv_sec*1000 + (double)last_time.tv_usec/1000);
		min_time_has_passed = period > 100;

		if (min_time_has_passed) {
			spus = get_spus();
			ctxs = get_spu_contexts(period);
			procs = get_procs(ctxs);
			fill_spus_tids(spus, ctxs);
			last_time = current_time;
		}

		print_cpu_info(min_time_has_passed);
		print_spu_info(spus, min_time_has_passed);
		if (screen_mode == PER_CTX) {
			print_header(ctx_fields);
			dump_fields((void**)ctxs, ctx_fields);
		} else if (screen_mode == PER_SPU) {
			print_header(spu_fields);
			dump_fields((void**)spus, spu_fields);
		} else {
			print_header(proc_fields);
			dump_fields((void**)procs, proc_fields);
		}

		move(5, 0);
		refresh();
		ch = getch();

		switch (ch) {
			case 'h':
			case 'H':
			case '?': show_help(); break;

			case 'q':
			case 'Q': do_quit = 1; break;

			case 'c': screen_mode = PER_CTX;  break;
			case 's': screen_mode = PER_SPU;  break;
			case 'p': screen_mode = PER_PROC; break;

			case 'A': screen_mode==PER_CTX? set_ctx_sort_descending(0) :
				  screen_mode==PER_SPU? set_spu_sort_descending(0) :
				  set_proc_sort_descending(0);
				  break;

			case 'D': screen_mode==PER_CTX? set_ctx_sort_descending(1) :
				  screen_mode==PER_SPU? set_spu_sort_descending(1) :
				  set_proc_sort_descending(1);
				  break;

			case 'f': config_show( screen_mode==PER_CTX? ctx_fields :
				screen_mode==PER_SPU? spu_fields : proc_fields);
				break;
			case 'o': config_order(screen_mode==PER_CTX? ctx_fields :
				screen_mode==PER_SPU? spu_fields : proc_fields);
				break;
			case 'O': config_sort( screen_mode==PER_CTX? ctx_fields :
				screen_mode==PER_SPU? spu_fields : proc_fields);
				break;
		}
	}
	quit();
	return 0;
}
