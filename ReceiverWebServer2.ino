#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Webserver
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
WebServer server(80);

const char* ssid = "Redmi Note 7";
const char* password = "ed12b5fb753c";

#define BOTtoken "8646318276:AAHkenz4sggWfBjAsnWx-mh9xtzZi_TPhRc"
#define CHAT_ID "7541426217"

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

bool telegramTerkirim = false;
bool wifiTerhubung = false;
bool loraTerhubung = false;

//=============================
// OLED
//=============================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);

//=============================
// LoRa TTGO T-Beam
//=============================

#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_SS     18
#define LORA_RST    23
#define LORA_DIO0   26

#define LORA_FREQ   433E6

//=============================
// OLED I2C
//=============================

#define SDA_PIN 21
#define SCL_PIN 22

//=============================
// Buzzer
//=============================

#define BUZZER 25

//======================
// Data dari Transmitter
//======================
String tanggalGPS = "--/--/----";
String waktuGPS = "--:--:--";
double latitude = 0;
double longitude = 0;
float speedKmh = 0;
float roll = 0;
float pitch = 0;
int satelit = 0;
String status = "";
int rssi = 0;

//====================================================
// Halaman OLED & Monitoring
//====================================================

byte halamanOLED = 0;
unsigned long waktuHalaman = 0;
const unsigned long INTERVAL_HALAMAN = 2400;

unsigned long waktuDataTerakhir = 0;
const unsigned long TIMEOUT_LORA = 15000;

// WEB
void handleData()
{
    Serial.print("GET /data  ");
    Serial.println(millis());
    StaticJsonDocument<256> doc;
    doc["date"] = tanggalGPS;
    doc["time"] = waktuGPS;
    doc["lat"] = latitude;
    doc["lon"] = longitude;
    doc["speed"] = speedKmh;
    doc["roll"] = roll;
    doc["pitch"] = pitch;
    doc["status"] = status;
    doc["rssi"] = rssi;
    doc["sat"] = satelit;

    String json;
    serializeJson(doc, json);

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "-1");
    server.send(200, "application/json", json);
}

void downloadLog()
{
    File file = LittleFS.open("/gpslog.csv", "r");
    server.streamFile(file, "text/csv");
    file.close();
}

void setup()
{
    Serial.begin(115200);

    pinMode(BUZZER, OUTPUT);
    digitalWrite(BUZZER, LOW);

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        while (1);
    }

    display.clearDisplay();

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQ))
    {
        display.println("LoRa Error");
        display.display();
        while (1);
    }

    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(8);
    LoRa.setTxPower(20);
    LoRa.enableCrc();

    display.setTextColor(WHITE);
    display.setTextSize(1);

    WiFi.begin(ssid, password);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.println("Connecting");
    display.setCursor(0, 45);
    display.println("WiFi");
    display.display();

    unsigned long startWiFi = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startWiFi < 10000)
    {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        client.setInsecure();

        Serial.println("\nWiFi Connected");
        Serial.print("IP Address : ");
        Serial.println(WiFi.localIP());

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 20);
        display.println("WiFi");
        display.setCursor(0, 45);
        display.println("Connected");
        display.display();

        if (!LittleFS.begin(true))
        {
            Serial.println("LittleFS gagal");
            return;
        }

        server.on("/", HTTP_GET, []()
        {
            File file = LittleFS.open("/index.html", "r");
            server.streamFile(file, "text/html");
            file.close();
        });

        server.on("/marker-icon-green.png", HTTP_GET, []()
        {
            File file = LittleFS.open("/marker-icon-green.png", "r");
            server.streamFile(file, "image/png");
            file.close();
        });

        server.on("/marker-icon-yellow.png", HTTP_GET, []()
        {
            File file = LittleFS.open("/marker-icon-yellow.png", "r");
            server.streamFile(file, "image/png");
            file.close();
        });

        server.on("/marker-icon-red.png", HTTP_GET, []()
        {
            File file = LittleFS.open("/marker-icon-red.png", "r");
            server.streamFile(file, "image/png");
            file.close();
        });

        server.on("/marker-shadow.png", HTTP_GET, []()
        {
            File file = LittleFS.open("/marker-shadow.png", "r");
            server.streamFile(file, "image/png");
            file.close();
        });

        server.on("/script.js", HTTP_GET, []()
        {
            File file = LittleFS.open("/script.js", "r");
            server.streamFile(file, "application/javascript");
            file.close();
        });

        server.on("/style.css", HTTP_GET, []()
        {
            File file = LittleFS.open("/style.css", "r");
            server.streamFile(file, "text/css");
            file.close();
        });

        server.on("/data", handleData);
        server.on("/download", downloadLog);

        if (!LittleFS.exists("/gpslog.csv"))
        {
            File file = LittleFS.open("/gpslog.csv", FILE_WRITE);
            if (file)
            {
                file.println("Date,Time,Latitude,Longitude,Speed,Roll,Pitch,Satellite,RSSI,Status");
                file.close();
            }
        }

        server.begin();
        delay(1000);
    }
    else
    {
        Serial.println("\nWiFi tidak tersedia");

        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 20);
        display.println("WiFi");
        display.setCursor(0, 45);
        display.println("Offline");
        display.display();

        delay(1000);
    }
    wifiTerhubung = (WiFi.status() == WL_CONNECTED);

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Receiver Ready");
    display.display();

    Serial.println("Receiver Ready");
}

void cekWiFi()
{
    static unsigned long lastReconnect = 0;

    if (WiFi.status() != WL_CONNECTED)
    {
        if (millis() - lastReconnect > 10000)
        {
            lastReconnect = millis();
            Serial.println("Mencoba reconnect WiFi...");
            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }
}

//====================================================
// Menerima Data LoRa
//====================================================

bool terimaDataLoRa()
{
    int packetSize = LoRa.parsePacket();

    if (packetSize)
    {
        String data = "";

        while (LoRa.available())
        {
            data += (char)LoRa.read();
        }

        rssi = LoRa.packetRssi();

        // Cari 8 posisi koma dari string yang dikirim Transmitter
        int index1 = data.indexOf(',');
        int index2 = data.indexOf(',', index1 + 1);
        int index3 = data.indexOf(',', index2 + 1);
        int index4 = data.indexOf(',', index3 + 1);
        int index5 = data.indexOf(',', index4 + 1);
        int index6 = data.indexOf(',', index5 + 1);
        int index7 = data.indexOf(',', index6 + 1);
        int index8 = data.indexOf(',', index7 + 1);

        // Pastikan seluruh 8 koma ditemukan
        if (index1 != -1 && index2 != -1 && index3 != -1 &&
            index4 != -1 && index5 != -1 && index6 != -1 &&
            index7 != -1 && index8 != -1)
        {
            latitude   = data.substring(0, index1).toDouble();
            longitude  = data.substring(index1 + 1, index2).toDouble();
            speedKmh   = data.substring(index2 + 1, index3).toFloat();
            roll       = data.substring(index3 + 1, index4).toFloat();
            pitch      = data.substring(index4 + 1, index5).toFloat();
            satelit    = data.substring(index5 + 1, index6).toInt();
            status     = data.substring(index6 + 1, index7);
            tanggalGPS = data.substring(index7 + 1, index8);
            waktuGPS   = data.substring(index8 + 1);
            
            tanggalGPS.trim();
            waktuGPS.trim();
        }

        Serial.println("================================");
        Serial.println("DATA DITERIMA");
        Serial.print("Latitude  : "); Serial.println(latitude, 6);
        Serial.print("Longitude : "); Serial.println(longitude, 6);
        Serial.print("Speed     : "); Serial.println(speedKmh);
        Serial.print("Roll      : "); Serial.println(roll);
        Serial.print("Pitch     : "); Serial.println(pitch);
        Serial.print("Satelit   : "); Serial.println(satelit);
        Serial.print("Status    : "); Serial.println(status);
        Serial.print("RSSI      : "); Serial.print(rssi); Serial.println(" dBm");
        Serial.print("Tanggal   : "); Serial.println(tanggalGPS);
        Serial.print("Waktu GPS : "); Serial.println(waktuGPS);
        Serial.print("Kualitas  : "); Serial.println(kualitasSinyal());
        Serial.println("================================");

        waktuDataTerakhir = millis();
        loraTerhubung = true;

        return true;
    }

    return false;
}

String kualitasSinyal()
{
    if (rssi >= -70) return "Sangat Baik";
    if (rssi >= -90) return "Baik";
    if (rssi >= -110) return "Sedang";
    return "Lemah";
}

void updateHalaman()
{
    if (millis() - waktuHalaman >= INTERVAL_HALAMAN)
    {
        waktuHalaman = millis();
        halamanOLED++;
        if (halamanOLED > 1) halamanOLED = 0;
    }
}

void tampilSerial()
{
    Serial.println("----------- DATA KAPAL -----------");
    Serial.print("RSSI      : "); Serial.print(rssi); Serial.println(" dBm");
    Serial.print("Kualitas  : "); Serial.println(kualitasSinyal());
    Serial.print("Satelit   : "); Serial.println(satelit);
    Serial.print("Latitude  : "); Serial.println(latitude);
    Serial.print("Longitude : "); Serial.println(longitude);
    Serial.print("Kecepatan : "); Serial.print(speedKmh); Serial.println(" km/jam");
    Serial.print("Roll      : "); Serial.println(roll);
    Serial.print("Pitch     : "); Serial.println(pitch);
    Serial.print("Status    : "); Serial.println(status);
    Serial.println("----------------------------------");
}

void tampilOLED()
{
    if (!loraTerhubung)
    {
        display.clearDisplay();
        display.setTextSize(2);
        display.setCursor(0, 20);
        display.println("KONEKSI");
        display.setCursor(0, 45);
        display.println("LORA PUTUS");
        display.display();
        return;
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);

    display.print("LoRa:");
    display.print(loraTerhubung ? "OK" : "OFF");

    display.setCursor(64, 0);
    display.print("WiFi:");
    display.println(wifiTerhubung ? "OK" : "OFF");

    display.drawLine(0, 10, 127, 10, WHITE);

    if (halamanOLED == 0)
    {
        display.setCursor(0, 12);
        display.println("= PELACAK POMPONG =");
        display.print("Lat:"); display.println(latitude, 5);
        display.print("Lon:"); display.println(longitude, 5);
        display.print("Spd:"); display.print(speedKmh, 1); display.println(" km/h");
        display.print("Sat:"); display.println(satelit);
        display.print("Status:"); display.println(status);
    }
    else
    {
        display.setCursor(0, 12);
        display.println("= SENSOR =");
        display.print("Roll:"); display.println(roll, 1);
        display.print("Pitch:"); display.println(pitch, 1);
        display.print("RSSI:"); display.print(rssi); display.print(" dBm "); display.println(kualitasSinyal());
        display.print("Waktu GPS: "); display.println(waktuGPS);
    }

    display.display();
}

void alarmBuzzer()
{
        bool kondisiAlarm = (status == "SOS") ||
        (status == "BAHAYA") ||
        !loraTerhubung;
        
    if (kondisiAlarm)
    {
        if ((millis() / 5000) % 2 == 0)
        {
            digitalWrite(BUZZER, HIGH);
        }
        else
        {
            digitalWrite(BUZZER, LOW);
        }
    }
    else
    {
        digitalWrite(BUZZER, LOW);
    }
}

void cekKoneksi()
{
    loraTerhubung = (millis() - waktuDataTerakhir <= TIMEOUT_LORA);
}

void kirimTelegram()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("WiFi Offline - Telegram tidak dikirim");
        return;
    }

    String statusLoRa = loraTerhubung ? "Sinyal LoRa Terhubung" : "Sinyal LoRa Putus";

    String pesan = "";
    pesan += "LoRa Tracker PELACAK POMPONG\n\n";
    pesan += "Status LoRa : " + statusLoRa;
    pesan += "Latitude : " + String(latitude, 6);
    pesan += "\nLongitude : " + String(longitude, 6);
    pesan += "\nKecepatan : " + String(speedKmh, 1) + " km/h";
    pesan += "\nStatus : " + status;
    pesan += "\nRoll : " + String(roll, 1);
    pesan += "\nPitch : " + String(pitch, 1);
    pesan += "\nSatelit : " + String(satelit);
    pesan += "\nRSSI : " + String(rssi) + " dBm";
    pesan += "\n\nGoogle Maps\nhttps://maps.google.com/?q=" + String(latitude, 6) + "," + String(longitude, 6);

    bot.sendMessage(CHAT_ID, pesan, "");
}

void loop()
{
    wifiTerhubung = (WiFi.status() == WL_CONNECTED);
    cekWiFi();
    cekKoneksi();
    server.handleClient();

    updateHalaman();

    if (terimaDataLoRa())
    {
        tampilSerial();

        // Simpan ke LittleFS dengan format 7 Kolom sesuai Header CSV (Date, Time, Lat, Lon, Speed, RSSI, Status)
        File file = LittleFS.open("/gpslog.csv", FILE_APPEND);
        if (file)
        {
            // Format: Date,Time,Latitude,Longitude,Speed,Roll,Pitch,Satellite,RSSI,Status
            file.printf("%s,%s,%.6f,%.6f,%.1f,%.1f,%.1f,%d,%d,%s\n",
                        tanggalGPS.c_str(),
                        waktuGPS.c_str(),
                        latitude,
                        longitude,
                        speedKmh,
                        roll,
                        pitch,
                        satelit,
                        rssi,
                        status.c_str());
            file.close();
        }

        tampilOLED();
        alarmBuzzer();

        Serial.print("Timeout: "); Serial.println(millis() - waktuDataTerakhir);

        if (status == "SOS" || status == "BAHAYA" || !loraTerhubung)
        {
            if (!telegramTerkirim)
            {
                kirimTelegram();
                telegramTerkirim = true;
            }
        }
        else
        {
            telegramTerkirim = false;
        }
    }
}