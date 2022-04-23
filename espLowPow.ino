#include "jimlib.h"
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <MD5Builder.h>


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
	return;
	va_list args;
	va_start(args, format);
	char buf[256];
	vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);
	mqtt.publish("debug", buf);
	jw.udpDebug(buf);
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
	//s += "{\"SourceFile\":\"" __BASE_FILE__ "\"},";
	o += s;
	dbg("LOG: %s", o.c_str());
	WiFiClient client;
	if (!client.connect("54.188.66.93", 80) || client.printf(o.c_str()) <= 0) {  
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
	pinMode(18, OUTPUT);
	digitalWrite(18, 1);

	//delay(100);
	//jw.run();
	//Serial.println("setup() finished");
	//delay(100);
	//if (WiFi.status() != WL_CONNECTED) {
	//		esp_sleep_enable_timer_wakeup(300LL * uS_TO_S_FACTOR);
	//	esp_deep_sleep_start();
	//}
}

EggTimer sec(10000), minute(60000);
int loopCount = 0;


float avgAnalogRead(int pin) { 
	float bv = 0;
	const int avg = 1024;
	for (int i = 0; i < avg; i++) {
		bv += analogRead(pin);
	}
	return bv / avg;
}

EggTimer blink(100);
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
		float bv1 = avgAnalogRead(35);
		float bv2 = avgAnalogRead(33);
		if (remoteLog(Sfmt("{\"Tiedown.Battery1Voltage\":%.1f},"
			"{\"Tiedown.Battery2Voltage\":%.1f}\n", bv1, bv2))) { 
			dbg("SUCCESS, LIGHT SLEEPING MINUTE");
			esp_sleep_enable_timer_wakeup(60LL * uS_TO_S_FACTOR);
			esp_light_sleep_start();
			dbg("SUCCESS, DEEP SLEEPING MINUTE");
			digitalWrite(pins.led, 0);
			pinMode(18, INPUT);
			esp_sleep_enable_timer_wakeup(3530LL * uS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}
	
		if(loopCount++ > 10) { 
			dbg("TOO MANY RETRIES, SLEEPING");
			digitalWrite(pins.led, 0);
			pinMode(18, INPUT);
			esp_sleep_enable_timer_wakeup(300LL * uS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}
	}
	delay(50);
}

