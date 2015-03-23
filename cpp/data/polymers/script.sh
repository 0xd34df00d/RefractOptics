#! /bin/sh

gnuplot << TOEND
set terminal epscairo enhanced font ",14"
set output '$1.eps'
set grid xtics ytics ztics
set xyplane 0
set xlabel "{/Symbol s}_{/Symbol l} / {/Symbol l}, × 10^{3}" offset 0,-1 font ",20"
set xtics offset 1,-0.5 0,0.2,0.9 font ",14"
set ylabel "{/Symbol s}_n / n, × 10^{3}" offset 0,-1 font ",20"
set ytics offset 1,-0.5 0,0.1 font ",14"
set zlabel "{/Symbol s}_{/Symbol w} / {/Symbol w}, × 10^{3}" offset -1.5,0 rotate by 90 font ",20"
set ztics font ",12"
unset key
unset colorbox
splot "$1" with pm3d
TOEND
