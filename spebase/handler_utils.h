/* handler_utils.h - definitions for assisted SPE library calls.
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

#ifndef __HANDLER_UTILS_H__
#define __HANDLER_UTILS_H__

struct spe_reg128 {
    unsigned int slot[4];
};

#ifndef LS_SIZE
#define LS_SIZE                       0x40000   /* 256K (in bytes) */
#define LS_ADDR_MASK                  (LS_SIZE - 1)
#endif /* LS_SIZE */

#define __PRINTF(fmt, args...) { fprintf(stderr,fmt , ## args); }
#ifdef DEBUG
#define DEBUG_PRINTF(fmt, args...) __PRINTF(fmt , ## args)
#else
#define DEBUG_PRINTF(fmt, args...)
#endif

#define LS_ARG_ADDR(_index)                             \
        (&((struct spe_reg128 *) ((char *) ls + ls_args))[_index])

#define DECL_RET()                              \
    struct spe_reg128 *ret = LS_ARG_ADDR(0)

#define GET_LS_PTR(_off)                        \
    (void *) ((char *) ls + ((_off) & LS_ADDR_MASK))

#define GET_LS_PTR_NULL(_off) \
       ((_off) ? GET_LS_PTR(_off) : NULL)

#define DECL_0_ARGS()                                   \
    unsigned int ls_args = (opdata & 0xffffff)

#define DECL_1_ARGS()                                   \
    DECL_0_ARGS();					\
    struct spe_reg128 *arg0 = LS_ARG_ADDR(0)

#define DECL_2_ARGS()                                   \
    DECL_1_ARGS();					\
    struct spe_reg128 *arg1 = LS_ARG_ADDR(1)

#define DECL_3_ARGS()                                   \
    DECL_2_ARGS();					\
    struct spe_reg128 *arg2 = LS_ARG_ADDR(2)

#define DECL_4_ARGS()                                   \
    DECL_3_ARGS();					\
    struct spe_reg128 *arg3 = LS_ARG_ADDR(3)

#define DECL_5_ARGS()                                   \
    DECL_4_ARGS();					\
    struct spe_reg128 *arg4 = LS_ARG_ADDR(4)

#define DECL_6_ARGS()                                   \
    DECL_5_ARGS();					\
    struct spe_reg128 *arg5 = LS_ARG_ADDR(5)

#define PUT_LS_RC(_a, _b, _c, _d)                       \
    ret->slot[0] = (unsigned int) (_a);                 \
    ret->slot[1] = (unsigned int) (_b);                 \
    ret->slot[2] = (unsigned int) (_c);                 \
    ret->slot[3] = (unsigned int) (_d);                 \
    __asm__ __volatile__ ("sync" : : : "memory")

#endif /* __HANDLER_UTILS_H__ */
