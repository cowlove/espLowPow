#!/usr/bin/python3
import sys
import re
import fileinput
import io 
import math

from datetime import datetime, timedelta 

def printPsychrData(t, dp):
    # k = log(humi/100) + (17.62 * temp) / (243.12 + temp);
    # dp 243.12 * k / (17.62 - k);
    b=17.625
    c=243.04

    # https://earthscience.stackexchange.com/questions/16570/how-to-calculate-relative-humidity-from-temperature-dew-point-and-pressure
    rh = 100 * math.exp( (c * b * (dp - t)) / ((c + t) * (c + dp)) )
    
    #https://en.wikipedia.org/wiki/Tetens_equation, saturation pressure in kPa
    satP = 0.61078 * math.exp((17.27 * t) / (t + 237.3)) * 7.50062
    
    vpd = (100 - rh) / 100 * satP;
    vp = rh / 100 * satP;
    print("%05.2f %05.2f %03.1f %05.2f %05.2f " % (t, dp, rh, vp, vpd), end='')


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
        print("%.3f %s %s" % (date.timestamp() / 86400), out ='');
        printPsychrData(float(m.group(2)), float(m.group(3)))
        # m.group(2), m.group(3)))


    # parse the plane output 
    m = re.search(r'\[(.*)\].*' + 
        r'Temp.:([-+0-9.]+).*Humidity.:([-+0-9.]+).*DewPoint.:([-+0-9.]+).*' +
        r'DessicantT.:([-+0-9.]+).*DessicantDP.:([-+0-9.]+).*' +
        r'OutsideT.:([-+0-9.]+).*OutsideDP.:([-+0-9.]+).*' +
        r'Voltage1.:([0-9.]+).*Voltage2.:([0-9.]+)', line)
    if m:
        if line.find("MOF-Guest") != -1:
            d = m.group(1)
            #2025-02-20T19:17:39.380Z 0 -999
            date = datetime.strptime(d, '%Y-%m-%dT%H:%M:%S.%fZ')
            date += timedelta(hours = -8)

            print("%.3f %.1f " % (date.timestamp() / 86400, float(m.group(3))), end='')
            printPsychrData(float(m.group(2)), float(m.group(4)))
            printPsychrData(float(m.group(5)), float(m.group(6)))
            printPsychrData(float(m.group(7)), float(m.group(8)))
            print("%.1f %.1f" % (float(m.group(9)), float(m.group(10))))

            #m.group(2), m.group(3), m.group(5), m.group(7),m.group(9), m.group(10)))
    else:    
        m = re.search(r'\[(.*)\].*Temp.:([-+0-9.]+).*DewPoint.:([-+0-9.]+).*' +
            r'Voltage1.:([0-9.]+).*' +
            r'Voltage2.:([0-9.]+)', line)
        if m:
            if line.find("MOF-Guest") != -1:
                d = m.group(1)
                #2025-02-20T19:17:39.380Z 0 -999
                date = datetime.strptime(d, '%Y-%m-%dT%H:%M:%S.%fZ')
                date += timedelta(hours = -8)
                print("%.3f " % (date.timestamp() / 86400), end='')
                printPsychrData(float(m.group(2)), float(m.group(3)))
                print("NaN NaN NaN NaN %s %s" % (m.group(4), m.group(5)))
