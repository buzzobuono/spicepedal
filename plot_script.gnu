set terminal qt persist
set title 'nope.csv - nope.csv
set xlabel 'Time [s]'
set ylabel 'Value'
set grid
set key outside right top
set datafile separator ';
plot 'nope.csv' using 1:2 with lines title 'V(1)', 'nope.csv' using 1:3 with lines title 'V(2)'
pause mouse close
