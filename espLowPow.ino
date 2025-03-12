#include "jimlib.h"

#ifndef UBUNTU
#include <ArduinoJson.h>

#include <esp_task_wdt.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <DHT.h>
#include <DHT_U.h>
#endif


struct {
	int led = getLedPin(); // D1 mini
	int fanPower = 13; 
	int fanPwm = 32;
	int bv1 = 35;
	int bv2 = 33;
    int dhtGnd = 25;
    int dhtVcc = 26;
    int dht1Data = 27;
    int dht2Data = 14;
    int dht3Data = 12;
} pins;

float calcDewpoint(float humi, float temp) {
  float k;
  k = log(humi/100) + (17.62 * temp) / (243.12 + temp);
  return 243.12 * k / (17.62 - k);
}

float calcWaterContent(float temp) { 
    return 5.04 + 0.24 * temp + 0.00945 * temp * temp + 0.000349 * temp * temp * temp;
}

JStuff j;
//DHT_Unified dht(pins.dhtData, DHT22);
DHT_Unified *dht1, *dht2, *dht3;

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout 

	if (getMacAddress() == "AC67B2368DFC") { 
		pins.fanPower = 0;
		pins.dhtGnd = 0;
		pins.dhtVcc = 22;
		pins.dht1Data = 21;
		pins.dht2Data = 16;
		pins.dht3Data = 17;
	}
	if (getMacAddress() == "A0DD6C725970") { 
		pins.dht2Data = pins.dht3Data = 0;
	}
		
	pinMode(pins.dhtGnd, OUTPUT);
	digitalWrite(pins.dhtGnd, 0);
	pinMode(pins.dhtVcc, OUTPUT);
	digitalWrite(pins.dhtVcc, 1);
	
	j.mqtt.active = false;
	j.begin();
	gpio_hold_dis((gpio_num_t)pins.fanPower);
	gpio_deep_sleep_hold_dis();

	pinMode(pins.fanPower, OUTPUT);
	pinMode(pins.led, OUTPUT);
	digitalWrite(pins.fanPower, 0);
	digitalWrite(pins.led, 0);

	ledcSetup(0, 25000, 6); // channel 0, 50 Hz, 16-bit width
	ledcAttachPin(pins.fanPwm, 0);

	delay(5000);
    
	dht1 = new DHT_Unified(pins.dht1Data, DHT22);
	dht2 = new DHT_Unified(pins.dht2Data, DHT22);
	dht3 = new DHT_Unified(pins.dht3Data, DHT22);
	dht1->begin();
	dht2->begin();
	dht3->begin();
}

Timer sec(2000), minute(60000);
Timer blink(100);
int loopCount = 0;
int firstLoop = 1;
float bv1, bv2;
int bv2Thresh = 1310;

struct DhtResult { 
	float temp, hum, dp, wc, vpd;
};

float vpd(float t, float rh) { 
	float sp = 0.61078 * exp((17.27 * t) / (t + 237.3)) * 7.50062;
	float vpd = (100 - rh) / 100 * sp;
	return vpd;
}

DhtResult readDht(DHT_Unified *dht, int n) { 
	DhtResult rval;
    sensors_event_t te, he;
	
	he.relative_humidity = te.temperature = -999;
	
	dht->temperature().getEvent(&te);
	dht->humidity().getEvent(&he);
	
	rval.dp = calcDewpoint(he.relative_humidity, te.temperature);
	rval.wc = calcWaterContent(rval.dp);
	rval.hum = he.relative_humidity;
	rval.temp = te.temperature;
	rval.vpd = vpd(rval.temp, rval.hum);

	OUT("%s %s N: %d T: %04.1f H: %04.1f D: %04.1f W: %04.1f V: %04.1f", getMacAddress().c_str(),
		WiFi.localIP().toString().c_str(), 
		n, rval.temp, rval.hum, rval.dp, rval.wc, rval.vpd);

	if (isnan(rval.hum)) rval.hum = -999;
	if (isnan(rval.temp)) rval.temp = -999;
	if (isnan(rval.dp)) rval.dp = -999;
	if (isnan(rval.wc)) rval.wc = -999;
	if (isnan(rval.vpd)) rval.vpd = -999;

	return rval;
}

DhtResult r1, r2, r3;
int fanMinutes = 0, fanPwm = 0, sleepMin = 0;

int postData(bool allowUpdate) { 
	int r = 0;
	String s;
	String mac = WiFi.macAddress();
	mac.replace(":", "");
	HTTPClient client;
	//WiFiClientSecure wc;
	//wc.setInsecure();
	//r = client.begin(wc, "http://vheavy.com/log");
	r = client.begin("http://vheavy.com/log");
	OUT("http.begin() returned %d", r);

	String spost = 
		Sfmt("{\"PROGRAM\":\"%s\",", basename_strip_ext(__BASE_FILE__).c_str()) + 
		Sfmt("\"GIT_VERSION\":\"%s\",", GIT_VERSION) + 
		Sfmt("\"MAC\":\"%s\",", mac.c_str()) + 
		Sfmt("\"SSID\":\"%s\",", WiFi.SSID().c_str()) + 
		Sfmt("\"IP\":\"%s\",", WiFi.localIP().toString().c_str()) + 
		Sfmt("\"RSSI\":%d,", WiFi.RSSI()) +
		Sfmt("\"Fan\":%d,", digitalRead(pins.fanPower)) + 
		Sfmt("\"Temp\":%.1f,", r1.temp) + 
		Sfmt("\"Humidity\":%.1f,", r1.hum) + 
		Sfmt("\"DewPoint\":%.1f,", r1.dp) + 
		Sfmt("\"VPD\":%.1f,", r1.vpd) + 
		Sfmt("\"WaterContent\":%.1f,", r1.wc) + 
		Sfmt("\"DessicantT\":%.1f,", r2.temp) + 
		Sfmt("\"DessicantDP\":%.1f,", r2.dp) + 
		Sfmt("\"OutsideT\":%.1f,", r3.temp) + 
		Sfmt("\"OutsideDP\":%.1f,", r3.dp) + 
		Sfmt("\"Voltage1\":%.1f,", bv1) + 
		Sfmt("\"Voltage2\":%.1f}\n", bv2);

	client.addHeader("Content-Type", "application/json");
	Serial.printf("POST %s\n", spost.c_str());
	r = client.POST(spost.c_str());
	s =  client.getString();
	client.end();
	OUT("http.POST returned %d: %s", r, s.c_str());
	
	StaticJsonDocument<1024> doc;
	DeserializationError error = deserializeJson(doc, s);
	const char *ota_ver = doc["ota_ver"];
	int status = doc["status"];
	fanPwm = doc["fanPwm"];
	fanMinutes = doc["fanMinutes"];
	sleepMin = doc["sleep"];

	if (ota_ver != NULL && allowUpdate) { 
		if (strcmp(ota_ver, GIT_VERSION) == 0 || strlen(ota_ver) == 0
			// dont update an existing -dirty unless ota_ver is also -dirty  
			//|| (strstr(GIT_VERSION, "-dirty") != NULL && strstr(ota_ver, "-dirty") == NULL)
			) {
			OUT("OTA version '%s', local version '%s', no upgrade needed", ota_ver, GIT_VERSION);
		} else { 
			OUT("OTA version '%s', local version '%s', upgrading...", ota_ver, GIT_VERSION);
			webUpgrade("http://vheavy.com/ota");
		}	
	}	  
	return status;
}

void loop() {
	j.run();

	if (blink.tick()) { 
		digitalWrite(pins.led, !digitalRead(pins.led));
	}
	if (!sec.tick()) {
		delay(100);
		return;
	}

	int status = 0;
	if (firstLoop) { 
		firstLoop = 0;
		bv1 = avgAnalogRead(pins.bv1);
		bv2 = avgAnalogRead(pins.bv2);

		r1 = readDht(dht1, 1);
		r2 = readDht(dht2, 2);
		r3 = readDht(dht3, 3);
	}
	if (WiFi.status() == WL_CONNECTED) {
		status = postData(true);
	
		if (status == 1 && fanMinutes > 0) {
			OUT("Turning on fan for %d minutes", fanMinutes);
			ledcWrite(0, 0);
			delay(500);
			pinMode(pins.fanPower, OUTPUT);
			digitalWrite(pins.fanPower, 1);
			delay(2000);
			//WiFi.disconnect(true);  // Disconnect from the network
			//WiFi.mode(WIFI_OFF);    // Switch WiFi off

			ledcWrite(0, fanPwm);
			for(int sec = 0; sec < 60 * fanMinutes; sec++) {
				j.run();
				delay(1000); 
			}
			bv1 = avgAnalogRead(pins.bv1);
			bv2 = avgAnalogRead(pins.bv2);
	
			r1 = readDht(dht1, 1);
			r2 = readDht(dht2, 2);
			r3 = readDht(dht3, 3);
			for(int retries = 0; retries < 10; retries++) {
				j.run(); 
				status = postData(false);
				if (status == 1)
					break;
			}
			j.run();
			ledcWrite(0, 0);
			delay(6000);
			digitalWrite(pins.fanPower, 0);
			WiFi.disconnect(true);  // Disconnect from the network
			WiFi.mode(WIFI_OFF);    // Switch WiFi off
			delay(1000);
			ESP.restart();                        
		}

		if (status == 1) {
			sleepMin = max(1, sleepMin);
			//sleepMin = 1;
			OUT("SLEEPING %d MINUTES", sleepMin);
			delay(5000);
			//adc_power_off();
			WiFi.disconnect(true);  // Disconnect from the network
			WiFi.mode(WIFI_OFF);    // Switch WiFi off
			esp_sleep_enable_timer_wakeup(60LL * 1000000LL * sleepMin);
			delay(1000);
			esp_deep_sleep_start();									
			ESP.restart();					
		}
	}

	OUT("Web post failed #%d", loopCount);
	// changes to jimlib
	if(loopCount++ > 30) { 
		int failSleepMinutes = 10;
		OUT("Too many retries, sleeping %d minutes", failSleepMinutes);
		digitalWrite(pins.led, 0);
		WiFi.disconnect(true);  // Disconnect from the network
		WiFi.mode(WIFI_OFF);    // Switch WiFi off
		delay(100);
		//ESP.restart();					
		esp_sleep_enable_timer_wakeup(failSleepMinutes * 60LL * 1000000LL);
		esp_deep_sleep_start();
	}
}
