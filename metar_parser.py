#!/usr/bin/python3
import sys
import re
import fileinput
import io 

from datetime import datetime, timedelta 

date_str = 'Tue May 08 15:14:45 +0800 2012'
for line in	io.TextIOWrapper(sys.stdin.buffer, errors='ignore'):
    
    # parse metar output 
    m = re.search(r'[0-9]+\W+([^m]+m)\W+([-+0-9.]+)\W+([-+0-9.]+)', line)
    if m:
        #>>> date_str = 'Tue May 08 15:14:45 +0800 2012'
        #Feb 19, 12:25 pm
        d = m.group(1)
        date = datetime.strptime(d, '%b %d, %I:%M %p').replace(year=datetime.today().year)
        #print((m.group(1), date))
        print("%.3f %s %s" % (date.timestamp() / 3600, m.group(2), m.group(3)))


    # parse the plane output 
    m = re.search(r'\[(.*)\].*Temp.:([-+0-9.]+).*DewPoint.:([-+0-9.]+)', line)
    if m:
        if line.find("MOF-Guest") != -1:
            d = m.group(1)
            #2025-02-20T19:17:39.380Z 0 -999
            date = datetime.strptime(d, '%Y-%m-%dT%H:%M:%S.%fZ')
            date += timedelta(hours = -8)
            print("%.3f %s %s" % (date.timestamp() / 3600, m.group(2), m.group(3)))
        