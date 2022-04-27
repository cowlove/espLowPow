#include "jimlib.h"
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <ArduinoJson.h>



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

int currentLength, totalLength;
// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void updateFirmware(uint8_t *data, size_t len){
  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if(currentLength != totalLength) return;
  Update.end(true);
  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes 
  ESP.restart();
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


void loop() {
	esp_task_wdt_reset();
	jw.run();
	//mqtt.run();
    if (jw.updateInProgress) {
		return;
	}

	if (blink.tick()) { 
		digitalWrite(pins.led, !digitalRead(pins.led));
		dbg("adc: %6.1f %6.1f %s", avgAnalogRead(35), avgAnalogRead(33), _GIT_VERSION);
	}	
	
	if (sec.tick()) {

		float bv1 = avgAnalogRead(35);
		float bv2 = avgAnalogRead(33);
	
		HTTPClient client;
		client.begin("http://54.188.66.93/log");
	
		String s = Sfmt("{\"Tiedown.BatteryVoltage1\":%.1f,"
			"\"Tiedown.BatteryVoltage2\":%.1f}\n", bv1, bv2);
		client.addHeader("Content-Type", "application/json");
		int r = client.POST(s.c_str());
		s =  client.getString(); //"{\"ota_ver\":\"gps\",\"time\":1351824120,\"data\":[48.756080,2.302038]}\n";
		client.end();

		dbg("http.POST() returned %d and %s\n", r, s.c_str());
 		StaticJsonDocument<1024> doc;
		DeserializationError error = deserializeJson(doc, s);
		const char *ota_ver = doc["ota_ver"];
		if (ota_ver != NULL) { 
			if (strcmp(ota_ver, GIT_VERSION) == 0) {
				dbg("OTA version '%s', local version '%s', no upgrade needed\n", ota_ver, GIT_VERSION);
			} else { 
				client.begin("http://54.188.66.93/ota");
				int resp = client.GET();
				dbg("HTTPClient.get() returned %d\n", resp);
				if(resp == 200){
					// get length of document (is -1 when Server sends no Content-Length header)
					totalLength = client.getSize();
					// transfer to local variable
					int len = totalLength;
					// this is required to start firmware update process
					Update.begin(UPDATE_SIZE_UNKNOWN);
					Serial.printf("FW Size: %u\n",totalLength);
					// create buffer for read
					uint8_t buff[128] = { 0 };
					// get tcp stream
					WiFiClient * stream = client.getStreamPtr();
					// read all data from server
					Serial.println("Updating firmware...");
					while(client.connected() && (len > 0 || len == -1)) {
						// get available data size
						size_t size = stream->available();
						if(size) {
							// read up to 128 byte
							int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
							// pass to function
							updateFirmware(buff, c);
							if(len > 0) {
								len -= c;
							}
						}
						delay(1);
					}	
				}

			}	
		}	  

		if (0) { 
			dbg("SUCCESS, LIGHT SLEEPING MINUTE");
			delay(100);
			esp_sleep_enable_timer_wakeup(60LL * uS_TO_S_FACTOR);
			esp_light_sleep_start();
			dbg("SUCCESS, DEEP SLEEPING MINUTE");
			digitalWrite(pins.led, 0);
			pinMode(18, INPUT);
			delay(100);
			esp_sleep_enable_timer_wakeup(3530LL * uS_TO_S_FACTOR);
			esp_deep_sleep_start();
		}

		if (0) { 
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

