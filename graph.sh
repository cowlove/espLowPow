#!/bin/bash
scp ls:simplePost/screenlog.0 . 
grep 34865D5E8F6C screenlog.0 | rematch '(\d\d)T(\d\d):(\d\d).*age1":([0-9.]+).*age2":([0-9.]+)' > 1.dat ; gnuplot -e 'set ytic nomirror; set y2tic; p [28.7:*] "1.dat" u ($1+($2-7)/24+$3/24/60):4 w l lw 5 t "B1", "1.dat" u ($1+($2-7)/24+$3/24/60):5 w l lw 5 ax x1y2 t "B2"; pause 111'


