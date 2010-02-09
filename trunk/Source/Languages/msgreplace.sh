#!/bin/sh
for L in *.lang ; do
	echo $L
	./msgreplace.pl "$@" < $L > $L.new
	mv $L.new $L
done

