#!/bin/bash 
cd ~/simplePost
#scp ls:simplePost/screenlog.0 . 
grep 34865D5E8F6C screenlog.0 | ${HOME}/bin/rematch '(\d\d)-(\d\d)T(\d\d):(\d\d).*age1":([0-9.]+).*age2":([0-9.]+)' > 1.dat ; 

X='($1*30+$2+($3-7)/24+$4/24/60)'
STYLE="w l lw 2"
gnuplot > /dev/null 2>&1 <<EOF
set term png size 1280,960;  
set output "p.png"; 
set ytic nomirror; 
set y2tic; 

stats "1.dat" u ${X}:5 
p [STATS_max_x-2:*] "1.dat" u ${X}:5 ${STYLE} t "Li Ion Battery", \
    "1.dat" u ${X}:6 ${STYLE} ax x1y2 t "12V Battery", \
    2300 w l ax x1y1 t "" lc "black", 1310 w l ax x1y2 t "" lc "black";

EOF


