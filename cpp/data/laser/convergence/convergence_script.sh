#! /bin/sh

gnuplot << TOEND
#set terminal epscairo enhanced font ",14"
#set output '$1.eps'
set terminal pdf font ",14"
set output '$1.pdf'
set xtics offset 1,-0.5 font ",14"
set ytics offset 0,-0.0 font ",14"
set xlabel "k"
set ylabel "{/Symbol w}_i / {/Symbol w}_{0i}"
plot "$1" using (\$1):(\$4/\$4) with lines title "Классическое значение", "$1" using (\$1):(\$5/\$2) with lines title "g_0", "$1" using (\$1):(\$6/\$3) with lines title "{/Symbol a}_0", "$1" using (\$1):(\$7/\$4) with lines title "{/Symbol g}"
TOEND
