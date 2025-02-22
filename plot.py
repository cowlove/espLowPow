#!/usr/bin/python3

import os
import argparse
parser = argparse.ArgumentParser()
parser.add_argument("-m", "--metar", help="fetch and update metar data",
                    action="store_true")
args = parser.parse_args()


if args.metar:
    print("Fetching metar data...",end='', flush=True)
    os.system("./metar_fetch.py > ./data/metar-`date +%Y%m%d.%H%M%S`.txt")
    os.system("cat ./data/metar-* | ./metar_parser.py | sort -n | uniq > ./data/metar.dat")
    print("OK")

print("Fetching posted data...",end='', flush=True)
os.system("ssh ls grep BB0 simplePost/screenlog.0 | grep MOF-Guest > ./data/plane_data.txt")
os.system("cat ./data/plane_data.txt | ./metar_parser.py > ./data/plane.dat")
print("OK")

os.system('gnuplot -e "set terminal qt size 1200,800;p [483360:*][-1:15] \'./data/plane.dat\' u 1:3 w l, \'./data/metar.dat\' u 1:3 w l; pause 1111"')

