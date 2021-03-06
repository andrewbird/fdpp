#!/bin/bash

set -e
set -o pipefail

gen_calls_tmp() {
	grep ASMCFUNC $1 | cpp -P -I $srcdir -include unfar.h | nl -n ln -v 0
}

gen_plt_inc() {
	grep ") FAR " $1 | sed 's/\([0-9]\+\).\+ \(SEGM(\(.\+\))\).* \([^ (]\+\) *(.*/asmcfunc_f \4,\t\t\1,\t\3/'
	grep -v ") FAR " $1 | sed 's/\([0-9]\+\).\+ \(SEGM(\(.\+\))\).* \([^ (]\+\) *(.*/asmcfunc_n \4,\t\t\1,\t\3/'
}

gen_asms_tmp() {
	grep 'ASMFUNC\|ASMPASCAL' $1 | grep -v "//" | \
		cpp -P -I $srcdir -include unfar.h | nl -n ln -v 0
}

gen_plt_asmc() {
	grep ASMFUNC $1 | sed 's/\([0-9]\+\).\+ \([^ (]\+\) *(.\+/asmcsym \2,\t\t\1/'
}

gen_plt_asmp() {
	grep ASMPASCAL $1 | sed 's/\([0-9]\+\).\+ \([^ (]\+\) *(.\+/asmpsym \U\2,\t\t\1/'
}

case "$1" in
1)
	gen_calls_tmp $2
	;;
2)
	gen_asms_tmp $2
	;;
3)
	gen_plt_inc $2
	;;
4)
	gen_plt_asmc $2
	;;
5)
	gen_plt_asmp $2
	;;
esac
