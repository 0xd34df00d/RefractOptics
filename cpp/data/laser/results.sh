#! /bin/sh

gnuplot << TOEND
#set terminal epscairo enhanced font ",14"
#set output '$1.eps'
set terminal pdf font ",14"
set output '$1.pdf'
set xtics font ",12"
set ytics font ",12"
set xlabel "R_0"
set ylabel "y"
set key bottom right
set datafile separator ','
f(x, g, a, y) = y * (1 - x) / (1 + x) * (g / (a - log (x) / 300) - 1)
plot 'ldata.txt' using (\$1):(\$2):(\$1 > 0.6 ? 0.01 : 0.02):(\$2 * 0.02) with xyerrorbars title 'Экспериментальные данные', f(x, 0.00291712, 0.000221917, 101.543) title '{/Symbol w}^0', f(x, 0.00292006, 0.000215645, 100.324) title '{/Symbol w}'
TOEND
