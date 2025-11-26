set terminal pngcairo size 0,0 enhanced font 'Arial,10'
set output ''
set title 'bazz-fuss.csv - bazz-fuss.csv'
set xlabel 'Time [s]'
set ylabel 'Value'
set grid
set key outside right top
set datafile separator ';'
plot 'bazz-fuss.csv' using 1:2 with lines title 'V(5)', 'bazz-fuss.csv' using 1:3 with lines title 'V(7)'
