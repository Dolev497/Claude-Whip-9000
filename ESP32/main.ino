#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoHttpClient.h>
#include <ESP32Servo.h>

Servo myservo;
int servoPin = 25;
int HOME_ANGLE = 30;
int OPEN_ANGLE = 170;


// WIFI STUFF
const char* ssid     = "POORIA";
const char* password = "POORIA2345";
const uint16_t port = 5005;
const char* endpoint = "/whip";

IPAddress hostIP;
char hostStr[16];
WiFiClient wifi;
HttpClient* client = nullptr;
unsigned long lastWhip = 0;
bool wifiReady = false;


// MIC STUFF
static constexpr const size_t record_number     = 200;
static constexpr const size_t record_length     = 240;
static constexpr const size_t record_size       = record_number * record_length;
static constexpr const size_t record_samplerate = 44100;
static size_t rec_record_idx  = 2;
static size_t draw_record_idx = 0;
static int16_t *rec_data;


// Whipping stuff
int whipCounter = 0;



void servoTo(int angle) {
    angle = constrain(angle, 0, 200);
    myservo.write(angle);
    Serial.printf("moved to %d degrees\n", angle);
}

void setup() {
    Serial.begin(115200);
    Serial.println("=== SERVO 180 -> 0 TEST ===");
    
    pinMode(servoPin, OUTPUT);
    myservo.setPeriodHertz(50);
    int attached = myservo.attach(servoPin, 600, 2400);
    if (attached < 0) {
        Serial.println("attach FAILED");
    } else {
        Serial.printf("attach OK, using channel %d\n", attached);
    }

    servoTo(OPEN_ANGLE);
    delay(1000);


    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
        delay(200);
    }


    wifiReady = (WiFi.status() == WL_CONNECTED);
    Serial.println("WIFI READY: ");
    Serial.println(wifiReady ? "OK" : "FAIL");

    
    if (wifiReady) {
        hostIP = WiFi.gatewayIP();
        sprintf(hostStr, "%u.%u.%u.%u", hostIP[0], hostIP[1],
                hostIP[2], hostIP[3]);
        client = new HttpClient(wifi, hostStr, port);
        // Short timeout only guards the initial TCP connect/write path
        // inside beginRequest()/endRequest() — we never wait on a response.
        client->setTimeout(1000);
    }


    rec_data = (typeof(rec_data))heap_caps_malloc(record_size *
                                                      sizeof(int16_t),
                                                  MALLOC_CAP_8BIT);
    memset(rec_data, 0, record_size * sizeof(int16_t));
    
    StickCP2.Speaker.setVolume(255);
    StickCP2.Speaker.end();
    StickCP2.Mic.begin();

    servoTo(HOME_ANGLE);


    auto cfg = M5.config();
    cfg.internal_imu  = false;
    cfg.internal_rtc  = false;
    cfg.internal_spk  = false;
    cfg.internal_mic  = true;
    cfg.external_imu  = false;  // <- prevents Ex_I2C.begin(), frees GPIO32/33
    cfg.external_rtc  = false;
    cfg.output_power  = false;

    StickCP2.begin(cfg);

}

void sendWhip() {
    Serial.println("[sendWhip] enter");
    if (!wifiReady) {
        Serial.println("[sendWhip] wifi not ready, return");
        return;
    }

    Serial.println("[sendWhip] drew WHIP sent");

    if (hostIP == IPAddress(0, 0, 0, 0)) {
        Serial.println("[sendWhip] no host, return");
        return;
    }

    Serial.println("[sendWhip] beginRequest");
    client->beginRequest();
    Serial.println("[sendWhip] post");
    client->post(endpoint);
    if (!client->connected()) {
        Serial.println("[sendWhip] CONN FAIL");
        client->stop();
        return;
    }
    Serial.println("[sendWhip] endRequest");
    client->endRequest();
    client->stop();
    Serial.println("[sendWhip] done");
}


void loop(void) {
    static unsigned long last = 0;
    StickCP2.update();

    if (StickCP2.BtnA.wasPressed()) {
        Serial.println("[reboot] BtnA pressed, restarting...");
        servoTo(HOME_ANGLE);   // optional: park the servo before reboot
        delay(200);            // let Serial flush / servo settle
        ESP.restart();
    }

    unsigned long now = millis();
    if (now - last < 10) return;
    last = now;

    static unsigned long lastBeat = 0;
    if (now - lastBeat >= 1000) {
        lastBeat = now;
    }
    if (StickCP2.Mic.isEnabled()) {
        auto data = &rec_data[rec_record_idx * record_length];
        if (StickCP2.Mic.record(data, record_length, record_samplerate)) {
            int64_t sumSq = 0;
            for (size_t i = 0; i < record_length; ++i) {
                int64_t v = data[i];
                sumSq += v * v;
            }
            float meanSq = (float)((double)sumSq / record_length);
            if (meanSq < 1.0f) meanSq = 1.0f;
            float rms = sqrtf(meanSq);
            float db  = 20.0f * log10f(rms);
            if (db >= 80.0f) {
                if (millis() - lastWhip < 4000) {
                    Serial.println("[whip] ignored (debounce)");
                    return;
                }
                lastWhip = millis();
                Serial.printf("[loop] db=%.1f >= threshold, calling whip\n", db);

                sendWhip();
                if (whipCounter >= 2) {
                    Serial.println("Kinky..");
                    servoTo(OPEN_ANGLE);
                    delay(2000);
                    servoTo(HOME_ANGLE);
                }

                whipCounter += 1;

            }

            if (++draw_record_idx >= record_number) {
                draw_record_idx = 0;
            }
            if (++rec_record_idx >= record_number) {
                rec_record_idx = 0;
            }
        }
    }
}
