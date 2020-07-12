/* default_c99_handler.h - emulate SPE C99 library calls.
 *
 * Copyright (C) 2005 IBM Corp.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __DEFAULT_C99_HANDLER_H__
#define __DEFAULT_C99_HANDLER_H__

#define SPE_C99_CLASS           0x2100

extern int default_c99_handler(unsigned long *base, unsigned long args);

extern int default_c99_handler_clearerr(char *ls, unsigned long args);
extern int default_c99_handler_fclose(char *ls, unsigned long args);
extern int default_c99_handler_feof(char *ls, unsigned long args);
extern int default_c99_handler_ferror(char *ls, unsigned long args);
extern int default_c99_handler_fflush(char *ls, unsigned long args);
extern int default_c99_handler_fgetc(char *ls, unsigned long args);
extern int default_c99_handler_fgetpos(char *ls, unsigned long args);
extern int default_c99_handler_fgets(char *ls, unsigned long args);
extern int default_c99_handler_fileno(char *ls, unsigned long args);
extern int default_c99_handler_fopen(char *ls, unsigned long args);
extern int default_c99_handler_fputc(char *ls, unsigned long args);
extern int default_c99_handler_fputs(char *ls, unsigned long args);
extern int default_c99_handler_fread(char *ls, unsigned long args);
extern int default_c99_handler_freopen(char *ls, unsigned long args);
extern int default_c99_handler_fseek(char *ls, unsigned long args);
extern int default_c99_handler_fsetpos(char *ls, unsigned long args);
extern int default_c99_handler_ftell(char *ls, unsigned long args);
extern int default_c99_handler_fwrite(char *ls, unsigned long args);
extern int default_c99_handler_getc(char *ls, unsigned long args);
extern int default_c99_handler_getchar(char *ls, unsigned long args);
extern int default_c99_handler_getenv(char *ls, unsigned long args);
extern int default_c99_handler_gets(char *ls, unsigned long args);
extern int default_c99_handler_perror(char *ls, unsigned long args);
extern int default_c99_handler_putc(char *ls, unsigned long args);
extern int default_c99_handler_putchar(char *ls, unsigned long args);
extern int default_c99_handler_puts(char *ls, unsigned long args);
extern int default_c99_handler_remove(char *ls, unsigned long args);
extern int default_c99_handler_rename(char *ls, unsigned long args);
extern int default_c99_handler_rewind(char *ls, unsigned long args);
extern int default_c99_handler_setbuf(char *ls, unsigned long args);
extern int default_c99_handler_setvbuf(char *ls, unsigned long args);
extern int default_c99_handler_system(char *ls, unsigned long args);
extern int default_c99_handler_tmpfile(char *ls, unsigned long args);
extern int default_c99_handler_tmpnam(char *ls, unsigned long args);
extern int default_c99_handler_ungetc(char *ls, unsigned long args);
extern int default_c99_handler_vfprintf(char *ls, unsigned long args);
extern int default_c99_handler_vfscanf(char *ls, unsigned long args);
extern int default_c99_handler_vprintf(char *ls, unsigned long args);
extern int default_c99_handler_vscanf(char *ls, unsigned long args);
extern int default_c99_handler_vsnprintf(char *ls, unsigned long args);
extern int default_c99_handler_vsprintf(char *ls, unsigned long args);
extern int default_c99_handler_vsscanf(char *ls, unsigned long args);

#endif /* __DEFAULT_C99_HANDLER_H__ */
