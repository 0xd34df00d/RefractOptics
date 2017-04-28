#! /bin/sh

TARGET=../ipi/Rudoy2016LevMar

mkdir -p $TARGET
cp bibliography.bib $TARGET
cp slashbox.sty $TARGET
cp authors.txt $TARGET/authors.txt
cp Rudoy2014LevMar.tex $TARGET/Rudoy2016LevMar.tex				# meh
cp Rudoy2014LevMar.pdf $TARGET/../Rudoy2016LevMar.pdf

mkdir -p $TARGET/figs/levmar/convergence
mkdir -p $TARGET/figs/levmar/comparison
cp figs/levmar/results.pdf $TARGET/figs/levmar
cp figs/levmar/results_bw.pdf $TARGET/figs/levmar
cp figs/levmar/convergence/*.pdf $TARGET/figs/levmar/convergence
cp figs/levmar/comparison/*.pdf $TARGET/figs/levmar/comparison

cd ../ipi
zip -r Rudoy2016LevMar Rudoy2016LevMar/
