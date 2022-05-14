#include "jimlib.h"
#include <ArduinoJson.h>

#include <esp_task_wdt.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

//#include "PubSubClient.h"
//#include "ArduinoOTA.h"

int getLedPin() { 
	const String mac = getMacAddress(); 
	if (mac == "9C9C1FC9BE94") return 2;
	return 2;	
}

struct {
	int led = getLedPin(); // D1 mini
 	int powerControlPin = 18;
	int fanPower = 27;
	int solarPwm = 25;
	int bv1 = 35;
	int bv2 = 33;
} pins;

MQTTClient mqtt("192.168.4.1", "lowpow", [](char* topic, byte* p, unsigned int l) {
		String s = buf2str(p, l);
	});

void test(); 	
void setup() {
	esp_task_wdt_init(60, true);
	esp_task_wdt_add(NULL);

	Serial.begin(921600, SERIAL_8N1);
	Serial.println("Restart");	

	gpio_hold_dis((gpio_num_t)pins.powerControlPin);
	gpio_hold_dis((gpio_num_t)pins.fanPower);
	gpio_deep_sleep_hold_dis();

	pinMode(pins.powerControlPin, OUTPUT);
	pinMode(pins.fanPower, OUTPUT);
	pinMode(pins.solarPwm, OUTPUT);
	pinMode(pins.led, OUTPUT);
	digitalWrite(pins.powerControlPin, 0);
	digitalWrite(pins.fanPower, 0);
	digitalWrite(pins.led, 1);

	ledcSetup(0, 50, 16); // channel 0, 50 Hz, 16-bit width
	ledcAttachPin(pins.solarPwm, 0);

	test();
	WiFiAutoConnect();
	ArduinoOTA.begin();
}

EggTimer sec(2000), minute(60000);
EggTimer blink(100);
int loopCount = 0;
int firstLoop = 1;
float bv1, bv2;

int bv2Thresh = 1310;

void loop() {
	esp_task_wdt_reset();
	//jw.run();
	mqtt.run();
	ArduinoOTA.handle();
    //if (jw.updateInProgress) {
	//		return;
	//}

	if (!sec.tick()) {
		delay(100);
		return;
	}

	int status = 0;
	if (firstLoop) { 
		bv1 = avgAnalogRead(pins.bv1);
		bv2 = avgAnalogRead(pins.bv2);
		firstLoop = 0;
	}
	if (WiFi.status() == WL_CONNECTED) {
		if (bv1 > 1000 && bv1 < 2300) {
			pinMode(pins.powerControlPin, OUTPUT);
			digitalWrite(pins.powerControlPin, 1);
			gpio_hold_en((gpio_num_t)pins.powerControlPin);
			gpio_deep_sleep_hold_en();
		}
		if (bv2 > bv2Thresh) {
			pinMode(pins.fanPower, OUTPUT);
			digitalWrite(pins.fanPower, 1);
			gpio_hold_en((gpio_num_t)pins.fanPower);
			gpio_deep_sleep_hold_en();
		}
		WiFiClientSecure wc;
		wc.setInsecure();
		//wc.setFingerprint(fingerprint);
		
		int r = 0;
		String s;
		if (1) { 
			HTTPClient client;
			r = client.begin("http://vheavy.ddns.net/log");
			dbg("http.begin() returned %d\n", r);
		
			String mac = WiFi.macAddress();
			mac.replace(":", "");
			s = Sfmt("{\"ddns\":1,\"GIT_VERSION\":\"%s\",", GIT_VERSION) + 
				Sfmt("\"MAC\":\"%s\",", mac.c_str()) + 
				Sfmt("\"Pow\":%d,", digitalRead(pins.powerControlPin)) + 
				Sfmt("\"Fan\":%d,", digitalRead(pins.fanPower)) + 
				Sfmt("\"Voltage1\":%.1f,", bv1) + 
				Sfmt("\"Voltage2\":%.1f}\n", bv2);

			client.addHeader("Content-Type", "application/json");
			r = client.POST(s.c_str());
			s =  client.getString();
			client.end();
		
			dbg("http.POST() returned %d and %s\n", r, s.c_str());
		}
		if (r != 200) { 
			HTTPClient client;
			r = client.begin(wc, "https://thingproxy.freeboard.io/fetch/https://vheavy.com/log");
			dbg("http.begin() returned %d\n", r);
		
			String mac = WiFi.macAddress();
			mac.replace(":", "");
			s = Sfmt("{\"GIT_VERSION\":\"%s\",", GIT_VERSION) + 
				Sfmt("\"MAC\":\"%s\",", mac.c_str()) + 
				Sfmt("\"Pow\":%d,", digitalRead(pins.powerControlPin)) + 
				Sfmt("\"Fan\":%d,", digitalRead(pins.fanPower)) + 
				Sfmt("\"Voltage1\":%.1f,", bv1) + 
				Sfmt("\"Voltage2\":%.1f}\n", bv2);

			client.addHeader("Content-Type", "application/json");
			r = client.POST(s.c_str());
			s =  client.getString();
			client.end();
		
			dbg("http.POST() returned %d and %s\n", r, s.c_str());
			
		}
		
		StaticJsonDocument<1024> doc;
		DeserializationError error = deserializeJson(doc, s);
		const char *ota_ver = doc["ota_ver"];
		status = doc["status"];
		const int pwm = doc["pwm"];
		const int pwmEnd = doc["pwmEnd"];
		int sleepMin = doc["sleep"];

		if (ota_ver != NULL) { 
			if (strcmp(ota_ver, GIT_VERSION) == 0
				// dont update an existing -dirty unless ota_ver is also -dirty  
				|| (strstr(GIT_VERSION, "-dirty") != NULL && strstr(ota_ver, "-dirty") == NULL)
				) {
				dbg("OTA version '%s', local version '%s', no upgrade needed\n", ota_ver, GIT_VERSION);
			} else { 
				dbg("OTA version '%s', local version '%s', upgrading...\n", ota_ver, GIT_VERSION);
				webUpgrade("https://thingproxy.freeboard.io/fetch/https://vheavy.com/ota");
			}	
		}	  

		if (status == 1) {
			if (0 && pwm > 500) { 
				pinMode(pins.powerControlPin, OUTPUT);
				digitalWrite(pins.powerControlPin, 1);
				delay(100);
				ledcWrite(0, pwm * 4715 / 1500);
				if (pwmEnd > 500) { 
					for (int n = pwm; n != pwmEnd; n += (pwmEnd > pwm ? 1 : -1)) { 
						ledcWrite(0, n * 4715 / 1500);
						delay(10);
					}
				}
				delay(1000);
				ledcDetachPin(pins.solarPwm);
				pinMode(pins.solarPwm, INPUT);
			}
			if (bv2 <= bv2Thresh) { 
				gpio_hold_dis((gpio_num_t)pins.powerControlPin);
				digitalWrite(pins.powerControlPin, 0);
				pinMode(pins.powerControlPin, INPUT);
			}
			sleepMin = max(10, sleepMin);
			dbg("SLEEPING %d MINUTES", sleepMin);
			//adc_power_off();
			WiFi.disconnect(true);  // Disconnect from the network
			WiFi.mode(WIFI_OFF);    // Switch WiFi off

			esp_sleep_enable_timer_wakeup(60LL * 1000000LL * sleepMin);
			delay(100);
			esp_deep_sleep_start();									
			ESP.restart();					
		}
	}

	if(loopCount++ > 30) { 
		dbg("TOO MANY RETRIES, SLEEPING");
		digitalWrite(pins.led, 0);
		pinMode(pins.powerControlPin, INPUT);
		delay(100);
		esp_sleep_enable_timer_wakeup(300LL * 1000000LL);
		esp_deep_sleep_start();
	}
}

void test() { 
	if (0) { 
		pinMode(pins.fanPower, OUTPUT);
		digitalWrite(pins.fanPower, 1);
		gpio_hold_en((gpio_num_t)pins.fanPower);
		//gpio_deep_sleep_hold_en();

		pinMode(pins.led, OUTPUT);
		digitalWrite(pins.led, 1);
		gpio_hold_en((gpio_num_t)pins.led);

		esp_sleep_enable_timer_wakeup(23LL * 60 * 1000000LL);

		dbg("SLEEPING");

		delay(100);
		esp_deep_sleep_start();									
	}

	while(0) { 
		LineBuffer lb;
		while (Serial.available()) { 
			lb.add(Serial.read(), [] (const char *l){
				int start, end, del;
				if (sscanf(l, "%d %d %d", &start, &end, &del) == 3) {
					Serial.printf("Got %d %d %d\n", start,end, del); 
					for (int n = start; n != end; n += (end > start ? 1 : -1)) { 
						ledcWrite(0, n * 4915 / 1500);
						delay(del);

					}
					for (int n = end; n != start; n += (end > start ? -1 : 	1)) { 
						ledcWrite(0, n * 4915 / 1500);
						delay(del);

					}
				}

			}); 
		}
		esp_task_wdt_reset();
		delay(1);
	}

	if (0) { 
		ledcWrite(0, 00 * 4915 / 1500);
		pinMode(pins.fanPower, OUTPUT);
		digitalWrite(pins.fanPower, !digitalRead(pins.fanPower));
		dbg("OUTPUT %d\n", digitalRead(pins.fanPower));
	
		//dbg("%d %s    " __BASE_FILE__ "   " __DATE__ "   " __TIME__, (int)(millis() / 1000), WiFi.localIP().toString().c_str());

		//delay(5000);
		dbg("pwm on");
		//ledcWrite(0, 800 * 4915 / 1500);

		
		//delay(5000);
		if (digitalRead(pins.fanPower) == 1) {
			if (millis() < 20000) {  
				return;
			}
			gpio_hold_en((gpio_num_t)pins.fanPower);
			gpio_deep_sleep_hold_en();
			dbg("sleep");
			delay(100);
		
			esp_sleep_enable_timer_wakeup(23LL * 60 * 1000000LL);
			esp_deep_sleep_start();
		
		
			dbg("fan on");
			ledcWrite(0, 2000 * 4915 / 1500);
			delay(150);
			ledcWrite(0, 1050 * 4915 / 1500);
			for(int s = 0; s < 60; s++) { 
				delay(1000);
				esp_task_wdt_reset();
			}
			dbg("fan off");

		}
	
		if (0) { 
			delay(1000);
			gpio_hold_en((gpio_num_t)pins.fanPower);
			gpio_deep_sleep_hold_en();
			delay(100);
			esp_sleep_enable_timer_wakeup(3LL * 1000000LL);
			//esp_light_sleep_start();
			gpio_hold_dis((gpio_num_t)pins.fanPower);
		}
		return;
	}		


}

