#!/usr/bin/python3

import os
import argparse

abspath = os.path.abspath(__file__)
dname = os.path.dirname(abspath)
os.chdir(dname)


parser = argparse.ArgumentParser()
parser.add_argument("-m", "--metar", help="fetch and update metar data", action="store_true")
parser.add_argument("-q", "--quick", help="don't fetch any external data, just plot", action="store_true")
args = parser.parse_args()


if args.metar:
    print("Fetching metar data...",end='', flush=True)
    os.system("./metar_fetch.py > ./data/metar-`date +%Y%m%d.%H%M%S`.txt")
    os.system("cat ./data/metar-* | ./metar_parser.py | sort -n | uniq > ./data/metar.dat")
    print("OK")

if not args.quick:
    print("Fetching posted data...",end='', flush=True)
    os.system("ssh ls grep BB0 simplePost/screenlog.0 | grep MOF-Guest > ./data/plane_data.txt")
    os.system("cat ./data/plane_data.txt | ./metar_parser.py > ./data/plane.dat")
    print("OK")

os.system('gnuplot -e "set grid; st=20156.2;f=\'./data/plane.dat\'; set terminal qt size 1200,800;' +
    'p [0:*][0:16]  ' +
    'f u (\\$1-st):7 w l title \'Cockpit Interior VDP\' lw 3' +
    ', f u (\\$1-st):17 w l title \'Ambient VPD\'' +
    ', f u (\\$1-st):12 w l title \'Dessicant Exhaust VDP\'' +
    ', f u (\\$1-st):20 w l title \'Fan\'' +
    ', f u (\\$1-st):21 ax x1y2 w l title \'Fan PWM\'' +
    #', \'./data/metar.dat\' u 1:2 w l title \'METAR Dewpoint\';' + 
    ', 4 w l title \'Threshold\'; pause 1111"')

