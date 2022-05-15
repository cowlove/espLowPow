#include "jimlib.h"
#include <ArduinoJson.h>

#include <esp_task_wdt.h>
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>

struct {
	int led = getLedPin(); // D1 mini
 	int powerControlPin = 18;
	int fanPower = 27;
	int solarPwm = 25;
	int bv1 = 35;
	int bv2 = 33;
} pins;

class JStuff {
public:
	JimWiFi jw;
	MQTTClient mqtt = MQTTClient("192.168.4.1", basename(__BASE_FILE__).c_str());
	void run() { 
		esp_task_wdt_reset();
		jw.run(); 
		mqtt.run(); 
	}
	void begin() { 
		esp_task_wdt_init(60, true);
		esp_task_wdt_add(NULL);

		Serial.begin(921600, SERIAL_8N1);
		Serial.println(__BASE_FILE__ " " GIT_VERSION);
		getLedPin();

		jw.onConnect([this](){
			jw.debug = mqtt.active = (WiFi.SSID() == "ChloeNet");
		});
		mqtt.setCallback([](String t, String m) {
			dbg("MQTT got string '%s'", m.c_str());
		});
	}
};

JStuff j;

void setup() {
	j.begin();
	gpio_hold_dis((gpio_num_t)pins.powerControlPin);
	gpio_hold_dis((gpio_num_t)pins.fanPower);
	gpio_deep_sleep_hold_dis();

	pinMode(pins.powerControlPin, OUTPUT);
	pinMode(pins.fanPower, OUTPUT);
	pinMode(pins.solarPwm, OUTPUT);
	pinMode(pins.led, OUTPUT);
	digitalWrite(pins.powerControlPin, 0);
	digitalWrite(pins.fanPower, 0);
	digitalWrite(pins.led, 0);

	ledcSetup(0, 50, 16); // channel 0, 50 Hz, 16-bit width
	ledcAttachPin(pins.solarPwm, 0);
}

EggTimer sec(2000), minute(60000);
EggTimer blink(100);
int loopCount = 0;
int firstLoop = 1;
float bv1, bv2;
int bv2Thresh = 1310;

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
		
		int r = 0;
		String s;
		String mac = WiFi.macAddress();
		mac.replace(":", "");
		HTTPClient client;
		WiFiClientSecure wc;
		wc.setInsecure();
		r = client.begin(wc, "https://thingproxy.freeboard.io/fetch/https://vheavy.com/log");
		dbg("http.begin() returned %d\n", r);
	
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
	// changes to jimlib
	if(loopCount++ > 30) { 
		dbg("TOO MANY RETRIES, SLEEPING");
		digitalWrite(pins.led, 0);
		pinMode(pins.powerControlPin, INPUT);
		delay(100);
		ESP.restart();					
		esp_sleep_enable_timer_wakeup(300LL * 1000000LL);
		esp_deep_sleep_start();
	}
}
