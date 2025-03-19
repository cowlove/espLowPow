#!/bin/bash

cd `dirname $0`
ST=20143.15
gnuplot -e "
    f='data/plane.dat'; 
    set y2tics;
    set ytics nomirror;
    set grid;
    plot [0:*] 
    f u (\$1-$ST):18 w l, 
    f u (\$1-$ST):19 w l ax x1y2; 
    pause 111

"
