#include "jimlib.h"
#include <PubSubClient.h>
#include <HTTPClient.h>

JimWiFi jw("MOF-Guest", "");
//JimWiFi jw;

struct {
	int led = 2;
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
			// Once connected, publish an announcement...
			String msg = "hello";
			client.publish((topicPrefix + "/debug").c_str(), msg.c_str());
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


#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP 30

void setup() {
	Serial.begin(921600, SERIAL_8N1);
	Serial.println("Restart");	
	
	esp_task_wdt_init(40, true);
	esp_task_wdt_add(NULL);

	pinMode(36, INPUT);
	//analogSetSamples(255);
	esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
	Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
  	delay(10);


	//Heltec.begin(true, true, true, true, 915E6);
	pinMode(pins.led, OUTPUT);
	digitalWrite(pins.led, 1);
	jw.onConnect([](void) { });
	jw.onOTA([](void) {});
	delay(100);
	jw.run();
	Serial.println("setup() finished");
	delay(100);
	if (WiFi.status() != WL_CONNECTED) {
		esp_deep_sleep_start();
		ESP.restart();
	}
}


EggTimer sec(200), minute(60000);
int first = 0;
int postCount = 0;
void loop() {
	esp_task_wdt_reset();
	Serial.printf("adc: %04d\n", analogRead(36));
	if (first == 0) Serial.println("runs()...");
	jw.run();
	//mqtt.run();
	if (first == 0) Serial.println("runs()...finished");
    if (jw.updateInProgress) {
		return;
	}

	first = 1;

	if (millis() > 60 * 1000) { // reboot every hour to keep OTA working? 
	}
	if (sec.tick()) {
		float bv = 0;
		const int avg = 500;
		for (int i = 0; i < avg; i++) {
			delay(1);
			bv += analogRead(36);
		}
		bv = bv / avg;

		WiFiClient client;
		HTTPClient http;

		// Your Domain name with URL path or IP address with path

		// Specify content-type header
		//http.addHeader("Content-Type", "application/json");
		//int r = http.POST(Sfmt("{\"Tiedown.BatteryVoltage\":%.1f}", bv));
		int r = -1;
		if (client.connect("54.188.66.93", 80)) {
			r = client.printf("{\"Tiedown.BatteryVoltage\":%.1f}\n", bv);
		}
		client.stop();
		delay(200);

	    //int r = http.GET();
		static int loopCount = 0;
		if(loopCount++ > 100) { 
			dbg("SLEEPING");
			esp_deep_sleep_start();
		}
		digitalWrite(pins.led, !digitalRead(pins.led));
		dbg("LOOP %d %.1f %d", loopCount, bv, r);
		if (r > 0) {
			if (++postCount > 2) {
				postCount = 0;
				dbg("SLEEPING");
				esp_sleep_enable_timer_wakeup(600 * uS_TO_S_FACTOR);
				esp_deep_sleep_start();
			}
		}
	}
	delay(10);
}

