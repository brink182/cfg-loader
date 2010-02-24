#!/bin/sh
perl -e '@file=<>;print "\xEF\xBB\xBF";print(@file);'
