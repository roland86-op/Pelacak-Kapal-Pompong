#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <TinyGPS++.h>
#include <MPU9250_asukiaaa.h>

#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

//======================
// Konfigurasi LoRa
//======================

#define LORA_SCK    5
#define LORA_MISO   19
#define LORA_MOSI   27
#define LORA_SS     18
#define LORA_RST    23
#define LORA_DIO0   26

#define LORA_FREQ   433E6

//======================
// GPS TTGO T-Beam
//======================

#define GPS_RX      34
#define GPS_TX      12

HardwareSerial GPS(1);
TinyGPSPlus gps;

//======================
// I2C GY-9250
//======================

#define SDA_PIN     21
#define SCL_PIN     22

MPU9250_asukiaaa mpu;
XPowersPMU power;
#define AXP2101_SLAVE_ADDRESS 0x34
//======================
// Tombol SOS
//======================

#define SOS_BUTTON 38

//======================
// Variabel GPS
//======================

double latitude = 0;
double longitude = 0;
double speedKmh = 0;
String tanggalGPS = "--/--/----";
String waktuGPS = "--:--:--";
//======================
// Variabel IMU
//======================

float roll = 0;
float pitch = 0;

//======================
// Moving Average
//======================

const int JUMLAH_SAMPEL = 5;

float rollBuffer[JUMLAH_SAMPEL];
float pitchBuffer[JUMLAH_SAMPEL];

int indexBuffer = 0;

//======================
// Status SOS
//======================

bool sos = false;

//======================
// Deteksi Bahaya
//======================

unsigned long waktuMiring = 0;
bool kondisiBahaya = false;


const float BATAS_ROLL = 30;
const float BATAS_PITCH = 45;     // derajat
const unsigned long WAKTU_BAHAYA = 3000;  // 3 detik

void setup()
{
    Serial.begin(115200);

    //=========================
    // Tombol SOS
    //=========================

    pinMode(SOS_BUTTON, INPUT_PULLUP);

    //=========================
    // I2C
    //=========================

    Wire.begin(SDA_PIN, SCL_PIN);

    //=========================
    // AXP2101
    //=========================

    initPMU();

    //=========================
    // MPU9250
    //=========================

    mpu.setWire(&Wire);

    mpu.beginAccel();
    mpu.beginGyro();

    if (mpu.accelUpdate() != 0)
    {
    Serial.println("GY-9250 tidak ditemukan!");

    while (1);
    }

    Serial.println("GY-9250 berhasil terhubung");

    //=========================
    // GPS
    //=========================

    GPS.begin(
        9600,
        SERIAL_8N1,
        GPS_RX,
        GPS_TX
    );

    Serial.println("GPS siap");

    //=========================
    // SPI LoRa
    //=========================

    SPI.begin(
        LORA_SCK,
        LORA_MISO,
        LORA_MOSI,
        LORA_SS
    );

    LoRa.setPins(
        LORA_SS,
        LORA_RST,
        LORA_DIO0
    );

    if (!LoRa.begin(LORA_FREQ))
    {
        Serial.println("LoRa gagal");

        while (1);
    }

    LoRa.setSpreadingFactor(12); /* 7-12 */
    LoRa.setSignalBandwidth(125E3); /* 500-31.25 */
    LoRa.setCodingRate4(8); /* 5-8 */
    LoRa.setTxPower(20);
    LoRa.enableCrc();
    
    Serial.println("LoRa siap");
}

void initPMU()
{
    bool result = power.begin(Wire, AXP2101_SLAVE_ADDRESS, SDA_PIN, SCL_PIN);

    if (!result)
    {
        Serial.println("AXP2101 gagal");
        while (1);
    }

    power.setALDO2Voltage(3300);
    power.setALDO3Voltage(3300);
    power.setALDO4Voltage(3300);

    power.enableALDO2();
    power.enableALDO3();
    power.enableALDO4();

    Serial.println("GPS Power ON");
}

//====================================================
// Membaca data GPS
//====================================================
void bacaGPS()
{
    while (GPS.available())
    {
        char c = GPS.read();
        Serial.write(c);      // tampilkan data mentah GPS
        gps.encode(c);
    }

    if (gps.time.isValid())
    {
        int jam = gps.time.hour() + 7;

        // Jika melewati 24 jam, kembali ke 0
        if (jam >= 24){jam -= 24;}

        char bufTanggal[15];
        char bufWaktu[10];

        sprintf(bufTanggal,
            "%02d-%02d-%04d",
            gps.date.day(),
            gps.date.month(),
            gps.date.year());

        sprintf(bufWaktu,
            "%02d:%02d:%02d",
            jam,
            gps.time.minute(),
            gps.time.second());

        tanggalGPS = String(bufTanggal);
        waktuGPS = String(bufWaktu);
    }
    else
    {
        tanggalGPS = "--/--/----";
        waktuGPS = "--:--:--";
    }

    if (!gps.location.isValid())
    {
        return;
    }

    latitude = gps.location.lat();
    longitude = gps.location.lng();

    if (gps.speed.isValid())
    {
        speedKmh = gps.speed.kmph();
    }

}

//====================================================
// Membaca MPU6050
//====================================================
void bacaMPU()
{
    mpu.accelUpdate();

    float ax = mpu.accelX();
    float ay = mpu.accelY();
    float az = mpu.accelZ();

    roll = atan2(ay, az) * 180.0 / PI;
    //roll += 160;
    pitch = 80+atan2(
                -ax,
                sqrt(ay * ay + az * az)
            ) * 180.0 / PI;
    //pitch += 80;
}

//====================================================
// Filter Moving Average
//====================================================

void movingAverage()
{
    rollBuffer[indexBuffer] = roll;
    pitchBuffer[indexBuffer] = pitch;

    indexBuffer++;

    if (indexBuffer >= JUMLAH_SAMPEL)
    {
        indexBuffer = 0;
    }

    float totalRoll = 0;
    float totalPitch = 0;

    for (int i = 0; i < JUMLAH_SAMPEL; i++)
    {
        totalRoll += rollBuffer[i];
        totalPitch += pitchBuffer[i];
    }

    roll = (totalRoll / JUMLAH_SAMPEL);
    pitch = (totalPitch / JUMLAH_SAMPEL);
}

//====================================================
// Membaca Tombol SOS
//====================================================
void bacaSOS()
{
    sos = (digitalRead(SOS_BUTTON) == LOW);
}

//====================================================
// Deteksi Kapal Miring
//====================================================

void deteksiBahaya()
{
    if (/*abs(roll) > BATAS_ROLL ||*/
        abs(pitch) > BATAS_PITCH)
    {
        if (waktuMiring == 0)
        {
            waktuMiring = millis();
        }

        if (millis() - waktuMiring >= WAKTU_BAHAYA)
        {
            kondisiBahaya = true;
        }
    }
    else
    {
        waktuMiring = 0;
        kondisiBahaya = false;
    }
}

//====================================================
// Menampilkan data ke Serial Monitor
//====================================================
void tampilSerial()
{
    Serial.println("--------------------------------");

    Serial.print("Latitude  : ");
    if (gps.location.isValid())
        Serial.println(gps.location.lat(), 6);
    else
        Serial.println("Belum Fix");

    Serial.print("Longitude : ");
    if (gps.location.isValid())
        Serial.println(gps.location.lng(), 6);
    else
        Serial.println("Belum Fix");

    Serial.print("Satelit   : ");
    if (gps.satellites.isValid())
        Serial.println(gps.satellites.value());
    else
        Serial.println("0");

    Serial.print("HDOP      : ");
    if (gps.hdop.isValid())
        Serial.println(gps.hdop.hdop());
    else
        Serial.println("-");

    Serial.print("Status    : ");
    if (gps.location.isValid())
        Serial.println("GPS FIX");
    else
        Serial.println("Mencari Satelit...");

    Serial.print("Kecepatan : ");
    Serial.print(speedKmh);
    Serial.println(" km/jam");

    Serial.print("Roll      : ");
    Serial.println(roll);

    Serial.print("Pitch     : ");
    Serial.println(pitch);

    Serial.print("Status Bahaya       : ");
    Serial.println(getStatus());

    Serial.println("--------------------------------");
}

//====================================================
// Menentukan Status Kapal
//====================================================

String getStatus()
{
    if (sos)
    {
        return "SOS";
    }

    if (kondisiBahaya)
    {
        return "BAHAYA";
    }

    return "NORMAL";
}

//====================================================
// Mengirim Data Melalui LoRa
//====================================================

void kirimDataLoRa()
{
    String status = getStatus();

    uint32_t satelit = gps.satellites.isValid() ?
                       gps.satellites.value() : 0;

    String data =
        String(latitude, 6) + "," +
        String(longitude, 6) + "," +
        String(speedKmh, 1) + "," +
        String(roll, 2) + "," +
        String(pitch, 2) + "," +
        String(satelit) + "," +
        status + "," +
        tanggalGPS + "," +
        waktuGPS;

    LoRa.beginPacket();
    LoRa.print(data);
    LoRa.endPacket();

    Serial.print("Data Terkirim : ");
    Serial.println(data);
}

//====================================================
// Program Utama
//====================================================

String waktuTerakhir = "";

void loop()
{
    bacaGPS();

    if (gps.time.isValid() && waktuGPS != waktuTerakhir)
    {
        waktuTerakhir = waktuGPS;

        bacaMPU();
        movingAverage();
        bacaSOS();
        deteksiBahaya();
        tampilSerial();

        kirimDataLoRa();
    }

    if (!gps.location.isValid())
    {
        Serial.println("Menunggu GPS Fix...");
        delay(1000);
        return;
    }
    delay(5000);
}
