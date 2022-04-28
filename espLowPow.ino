#include "jimlib.h"
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>




JimWiFi jw("MOF-Guest", "");
//JimWiFi jw;

struct {
	int led = 5;
	int rxHeater = 17;
	int txHeater = 16;
	int rxDisplay = 22;
	int dummy1 = 21;
	int dummy2 = 22;
} pins;

void mqttCallback(char* topic, byte* payload, unsigned int length);

class MQTTClient { 
	WiFiClient espClient;
	String topicPrefix, server;
public:
	PubSubClient client;
	MQTTClient(const char *s, const char *t) : server(s), topicPrefix(t), client(espClient) {}
	void publish(const char *suffix, const char *m) { 
		String t = topicPrefix + "/" + suffix;
		client.publish(t.c_str(), m);
	}
	void publish(const char *suffix, const String &m) {
		 publish(suffix, m.c_str()); 
	}
	void reconnect() {
	// Loop until we're reconnected
		if (WiFi.status() != WL_CONNECTED || client.connected()) 
			return;
		
		Serial.print("Attempting MQTT connection...");
		client.setServer(server.c_str(), 1883);
		client.setCallback(mqttCallback);
		if (client.connect(topicPrefix.c_str())) {
			Serial.println("connected");
			// ... and resubscribe
			client.subscribe((topicPrefix + "/in").c_str());
			client.setCallback(mqttCallback);
		} else {
			Serial.print("failed, rc=");
			Serial.print(client.state());
		}
	}
	void dprintf(const char *format, ...) { 
		va_list args;
		va_start(args, format);
        char buf[256];
        vsnprintf(buf, sizeof(buf), format, args);
	    va_end(args);
		client.publish((topicPrefix + "/debug").c_str(), buf);
	}
	void run() { 
		client.loop();
		reconnect();
	}
 };
 
MQTTClient mqtt("192.168.4.1", "lowpow");

void dbg(const char *format, ...) { 
	va_list args;
	va_start(args, format);
	char buf[256];
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	//mqtt.publish("debug", buf);
	//jw.udpDebug(buf);
	Serial.println(buf);
}


void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  std::string p;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
	p += (char)payload[i];
  }
  mqtt.publish("heater/out", "got mqtt message");
  Serial.println();
}

bool remoteLog(const String &s) {
	String mac = WiFi.macAddress();
	mac.replace(":", "");
	String o = String("{\"MAC\":\"" + mac + "\"},");
	//o += "{\"SourceFile\":\"" __BASE_FILE__ "\"},";
	o += s;
	dbg("LOG: %s", o.c_str());
	WiFiClient client;
	if (WiFi.status() != WL_CONNECTED) {
		dbg("NOT CONNECTED");
	}

	int r = client.connect("54.188.66.93", 80);
	dbg("connect() returned %d", r);
	r = client.print(o.c_str());
	if (r <= 0) {  
		dbg("write() failed with %d", r);
		return false;
	}
	String i = client.readStringUntil('\n');
	client.stop();
	MD5Builder md5;
	md5.begin();
	md5.add(o.c_str());
	md5.calculate();
	String hash = md5.toString();
	hash.toLowerCase();
	i.toLowerCase();
	dbg("MD5 CALC: '%s' IN: '%s'\n", hash.c_str(), i.c_str());
	return strstr(i.c_str(), hash.c_str()) != NULL;
}


#define uS_TO_S_FACTOR 1000000LL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 30

void setup() {
	Serial.begin(921600, SERIAL_8N1);
	Serial.println("Restart");	
	
	esp_task_wdt_init(30, true);
	esp_task_wdt_add(NULL);

	//pinMode(35, INPUT);
	//analogSetSamples(255);

	pinMode(pins.led, OUTPUT);
	digitalWrite(pins.led, 0);

	//delay(100);
	//jw.run();
	//Serial.println("setup() finished");
	//delay(100);
	//if (WiFi.status() != WL_CONNECTED) {
	//		esp_sleep_enable_timer_wakeup(300LL * uS_TO_S_FACTOR);
	//	esp_deep_sleep_start();
	//}
}

EggTimer sec(5000), minute(60000);
EggTimer blink(100);
int loopCount = 0;


float avgAnalogRead(int pin) { 
	float bv = 0;
	const int avg = 1024;
	for (int i = 0; i < avg; i++) {
		bv += analogRead(pin);
	}
	return bv / avg;
}

void webUpgrade(const char *url) {
	WiFiClientSecure wc;
	wc.setInsecure();
	HTTPClient client; 
	client.begin(wc, url);
	int resp = client.GET();
	dbg("HTTPClient.get() returned %d\n", resp);
	if(resp != 200) {
		dbg("Get failed\n");
		Serial.print(client.getString());
		delay(5000);
	} else { 
		int currentLength = 0;
		int	totalLength = client.getSize();
		int len = totalLength;
		uint8_t buff[128] = { 0 };
	
		Update.begin(UPDATE_SIZE_UNKNOWN);
		Serial.printf("FW Size: %u\n",totalLength);
		WiFiClient * stream = client.getStreamPtr();
		Serial.println("Updating firmware...");
		while(client.connected() && (len > 0 || len == -1)) {
			size_t size = stream->available();
			if(size) {
				esp_task_wdt_reset();
				int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
				Update.write(buff, c);
				Serial.print(".");
				currentLength += c;
				if(currentLength == totalLength) {
					Update.end(true);
					Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
					ESP.restart();
				}
				if(len > 0) {
					len -= c;
				}
			}
			delay(1);
		}	
	}
}


 
//const char *fingerprint="AD 1D 38 14 A8 11 BA E4 7C 63 2D 1F 8A 50 F9 C8 1E B1 FC 6D";
const char *fingerprint="AD:1D:38:14:A8:11:BA:E4:7C:63:2D:1F:8A:50:F9:C8:1E:B1:FC:6D";
//const char *fingerprint="AD1D3814A811BAE47C632D1F8A50F9C81EB1FC6D";	
//"AD:1D:38:14:A8:11:BA:E4:7C:63:2D:1F:8A:50:F9:C8:1E:B1:FC:6D"

int firstLoop = 1;
float bv1, bv2;
void loop() {
	esp_task_wdt_reset();
	jw.run();
	//mqtt.run();
    if (jw.updateInProgress) {
		return;
	}

	if (blink.tick()) { 
		digitalWrite(pins.led, !digitalRead(pins.led));
		dbg("adc: %6.1f %6.1f", avgAnalogRead(35), avgAnalogRead(33));
	}	
	
	if (sec.tick()) {
		int status = 0;
		if (firstLoop) { 
			bv1 = avgAnalogRead(35);
			bv2 = avgAnalogRead(33);
			firstLoop = 0;
			pinMode(18, OUTPUT);
			digitalWrite(18, 1);
		}
	
		WiFiClientSecure wc;
		wc.setInsecure();
		//wc.setFingerprint(fingerprint);
		HTTPClient client;
		int r = client.begin(wc, "https://54.188.66.93/log");
		dbg("http.begin() returned %d\n", r);
 	
	
		String mac = WiFi.macAddress();
		mac.replace(":", "");
		String s = Sfmt("{\"MAC\":\"%s\",\"Tiedown.BatteryVoltage1\":%.1f,"
			"\"Tiedown.BatteryVoltage2\":%.1f}\n", mac.c_str(), bv1, bv2);
		client.addHeader("Content-Type", "application/json");
		r = client.POST(s.c_str());
		//r = client.GET();
		s =  client.getString();
		client.end();
		if (WiFi.status() != WL_CONNECTED) {
			dbg("NOT CONNECTED");
		}

		dbg("http.POST() returned %d and %s\n", r, s.c_str());
 	
	 	StaticJsonDocument<1024> doc;
		DeserializationError error = deserializeJson(doc, s);
		const char *ota_ver = doc["ota_ver"];
		status = doc["status"];

		if (ota_ver != NULL) { 
			if (strcmp(ota_ver, GIT_VERSION) == 0) {
				dbg("OTA version '%s', local version '%s', no upgrade needed\n", ota_ver, GIT_VERSION);
			} else { 
				dbg("OTA version '%s', local version '%s', upgrading...\n", ota_ver, GIT_VERSION);
				while(1) { 
					webUpgrade("https://54.188.66.93/ota");
				}
			}	
		}	  

		if (status == 1) { 
			dbg("SUCCESS, LIGHT SLEEPING 5 MINUTES");
			delay(100);
			//adc_power_off();
			WiFi.disconnect(true);  // Disconnect from the network
			WiFi.mode(WIFI_OFF);    // Switch WiFi off
			esp_sleep_enable_timer_wakeup(300LL * uS_TO_S_FACTOR);
			esp_light_sleep_start();
			dbg("SUCCESS, DEEP SLEEPING FOR AN HOUR MINUTE");
			digitalWrite(pins.led, 0);
			pinMode(18, INPUT);
			delay(100);
			esp_sleep_enable_timer_wakeup(3530LL * uS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}

		if(loopCount++ > 10) { 
			if (WiFi.status() != WL_CONNECTED) {
				dbg("NEVER CONNECTED, REBOOTING");
				ESP.restart();
			}
			dbg("TOO MANY RETRIES, SLEEPING");
			digitalWrite(pins.led, 0);
			pinMode(18, INPUT);
			esp_sleep_enable_timer_wakeup(300LL * uS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}
	}
	delay(50);
}

