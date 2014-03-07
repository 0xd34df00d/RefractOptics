#! /bin/sh

gnuplot << TOEND
set terminal svg
set output '$1.svg'
plot "$1" using 1:2 with lines, "$1" using 1:3 with lines
set xrange [0:1000]
set output '$1_0_1000.svg'
plot "$1" using 1:2 with lines, "$1" using 1:3 with lines
set xrange [1000:10000]
set output '$1_1000_end.svg'
plot "$1" using 1:2 with lines, "$1" using 1:3 with lines
TOEND
