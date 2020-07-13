#!/bin/sh
#
#  libspe2 - A wrapper library to adapt the JSRE SPU usage model to SPUFS
#
#  Copyright (C) 2007 Sony Computer Entertainment Inc.
#  Copyright 2007 Sony Corp.
#
#  This library is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public
#  License as published by the Free Software Foundation; either
#  version 2.1 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this library; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#

loader="./test_c99_main.elf"
filename_rename_from=__c99_stdio_misc_rename_from.tmp
filename_rename_to=__c99_stdio_misc_rename_to.tmp
filename_remove=__c99_stdio_misc_remove.tmp

touch $filename_rename_from || exit 1
touch $filename_remove || exit 1
rm -f $filename_rename_to || exit 1

[ -f $filename_rename_from -a -f $filename_remove ] || exit 1
[ -e $filename_rename_to ] && exit 1

$loader spu_c99_stdio_misc.spu.elf || exit 1

[ -e $filename_rename_from -o -e $filename_remove ] && exit 1
[ -f $filename_rename_to ] || exit 1

rm -f $filename_rename_to
exit 0
