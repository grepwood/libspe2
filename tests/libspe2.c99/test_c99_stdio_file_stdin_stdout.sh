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
result=__c99_stdio_file_stdin_stdout_result.tmp
output_tmp=__c99_stdio_file_stdin_stdout_output.tmp

cat \
    data_c99_stdio_file_input.txt \
    data_c99_stdio_scanf_input.txt \
    data_c99_stdio_scanf_input.txt |
$loader spu_c99_stdio_file_stdin_stdout.spu.elf > $result || exit 1

cat \
    data_c99_stdio_file_output.txt \
    data_c99_stdio_printf_output.txt \
    data_c99_stdio_printf_output.txt > $output_tmp
diff $result $output_tmp || exit 1

rm -f $result $output_tmp
exit 0
