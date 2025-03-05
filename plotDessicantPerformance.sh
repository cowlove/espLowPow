#!/bin/bash

cd `dirname $0`

gnuplot -e "f='./data/plane.dat'; 
    ST=20143.15;
    set terminal qt size 1200,800;
    p [0:*][*:*] 
    f u (\$1-ST):(\$12-\$7) w l tit 'dVPD', 
    f u (\$1-ST):(\$8-\$3) w l tit 'dTemp'; 
    pause 1111; 
"