#!/bin/sh

die() {
    echo "gen-make-deps.sh:" "$@"
    exit 1
}

getdeps() {
    for depname in $(cpp -MM "$1" |cut -d: -f2-) ; do
	echo $depname
    done |grep '\.[ch]$'
}

makefile="$1";
if test -z "$makefile" ; then
    makefile="Makefile"
fi

if test ! -e "$makefile" ; then
    die "$makefile not found"
fi

modules=$(grep ^MODULES 2>/dev/null < "$makefile" |sed -e 's|\(.*\)=\(.*\)|\2|')
if test -z "$modules" ; then
    die "Modules not found"
fi
prefixes=$(echo $modules |sed -e "s|\.o||g")

tmp=$(tempfile) || die "Can not create a temporary file"
cat "$makefile" > "$tmp"
echo >> "$tmp"

for prefix in $prefixes ; do
    echo "$prefix.o: $(echo $(getdeps "$prefix.c"))" >> "$tmp"
done

mv -f "$tmp" "$makefile"
