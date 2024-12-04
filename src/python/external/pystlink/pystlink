#!/bin/bash

exec_file=$0

if [ -L "$exec_file" ] ; then
    exec_file=`readlink $exec_file`
fi

work_dir=`dirname $exec_file`

python3 $work_dir/pystlink.py $*
