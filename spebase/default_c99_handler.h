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

extern int _base_spe_default_c99_handler(unsigned long *base, unsigned long args);

#endif /* __DEFAULT_C99_HANDLER_H__ */
