#! /bin/sh

gnuplot << TOEND
set terminal pdfcairo enhanced font ",12"
set output '$1.pdf'
set grid xtics ytics ztics
set xyplane 0
set xlabel "{/Symbol s}_{/Symbol l}, × 10^{3}"
set xtics offset 1,-0.5 font ",8"
set ylabel "{/Symbol s}_n, × 10^{3}"
set ytics offset 1,-0.5 font ",8"
set zlabel "устойчивость, × 10^{3}" offset -2,0 rotate by 90
set ztics font ",8"
unset key
splot "$1" with pm3d
TOEND
