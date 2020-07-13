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
result=__c99_stdio_file_result.tmp
input_tmp=__c99_stdio_file_input.tmp
output_tmp=__c99_stdio_file_output.tmp
output_template=__c99_stdio_file_output_template.tmp

cat \
    data_c99_stdio_file_input.txt \
    data_c99_stdio_scanf_input.txt \
    data_c99_stdio_scanf_input.txt > $input_tmp

$loader spu_c99_stdio_file.spu.elf || exit 1

cat \
    data_c99_stdio_file_output.txt \
    data_c99_stdio_printf_output.txt \
    data_c99_stdio_printf_output.txt > $output_template

### generate check data
# default test
cat $output_template > $output_tmp || exit 1
# rewind test
cat $output_template >> $output_tmp || exit 1
# fseek test
tail -n +2 $output_template >> $output_tmp || exit 1
# fsetpos test
tail -n +2 $output_template >> $output_tmp || exit 1

# setvbuf test (NULL, unbufered)
cat $output_template >> $output_tmp || exit 1
# setvbuf test (NULL, line buffered)
cat $output_template >> $output_tmp || exit 1
# setvbuf test (NULL, full buffered)
cat $output_template >> $output_tmp || exit 1
# setvbuf test (non-NULL, full buffered)
cat $output_template >> $output_tmp || exit 1

# setbuf test (NULL)
cat $output_template >> $output_tmp || exit 1
# setbuf test (non-NULL)
cat $output_template >> $output_tmp || exit 1

# tmpfile test
cat $input_tmp >> $output_tmp || exit 1
# freopen test
cat $output_template >> $output_tmp || exit 1

### compare result
diff $result $output_tmp || exit 1

rm -f $result $input_tmp $output_tmp $output_template
exit 0
