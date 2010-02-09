#!/bin/sh
POT=$1

if [ -z "$POT" ]; then
  echo "usage: $0 langXX.pot"
  exit
fi

function del_bom
{
	perl -e '@file=<>;$file[0] =~ s/^\xEF\xBB\xBF//;print(@file);'
}

function add_bom
{
	perl -e '@file=<>;print "\xEF\xBB\xBF";print(@file);'
}

for L in *.lang ; do
	echo $L
	del_bom < $L | msgmerge.exe --sort-output --no-wrap --no-location -N - "$POT" | add_bom > $L.new
	mv $L.new $L
done

