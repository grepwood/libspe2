/* default_posix1_handler.h - emulate SPE C99 library calls.
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

#ifndef __DEFAULT_POSIX1_HANDLER_H__
#define __DEFAULT_POSIX1_HANDLER_H__

#define SPE_POSIX1_CLASS     0x2101

extern int _base_spe_default_posix1_handler(char *ls, unsigned long args);

#endif /* __DEFAULT_POSIX1_HANDLER_H__ */
