#!/usr/bin/python3

import re
import sys
import pandas
import time
import io
from selenium import webdriver

opt = webdriver.FirefoxOptions()
opt.set_preference("geo.enabled", False)
opt.add_argument("--headless")
opt.set_preference("browser.helperApps.neverAsk.saveToDisk", "application/sla")
try:
    driver = webdriver.Firefox( options=opt)  
except:
    serv = webdriver.FirefoxService( executable_path='/snap/bin/geckodriver' )
    driver = webdriver.Firefox( options=opt, service=serv)  

driver.get("https://www.weather.gov/wrh/timeseries?site=KBFI&hours=72&units=metric&chart=on&headers=on&obs=tabular&hourly=false&pview=standard")
time.sleep(6)
page_html = driver.page_source
driver.close()

tables = pandas.read_html(io.StringIO(page_html));
data = tables[1];
#print(data)
print(data.to_string())
if False:
    for line in sys.stdin:
        m = re.search(r'T:\s+([-]?[0-9.]+)\s+H:\s+(([-]?[0-9.]+))', line)
        if m: 
            t = float(m.group(1))
            h = float(m.group(2))
            print("%+05.1f\t%04.1f\t%+05.1f" % (t, h, t - (100 - h) / 5))



