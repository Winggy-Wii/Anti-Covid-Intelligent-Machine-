#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Stepper.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <WiFiClientSecure.h>
#include <CertStoreBearSSL.h>
#include <DNSServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "Adafruit_VL53L0X.h"
BearSSL::CertStore certStore;
#include <time.h>

Stepper my_Stepper(200, D5, D6, D7, D8);

Adafruit_VL53L0X lox = Adafruit_VL53L0X();

int PostitionSwitchA = digitalRead(D3);
int PostitionSwitchB = digitalRead(D4);

bool Initial = false;
bool PrevSwitchA = false;
bool PrevSwitchB = false;
int MeasureData = 0;
unsigned long DelayIndicator;
unsigned long DelayIndicatorLater = 0;

const char *PARAM_INPUT_1 = "output";
const char *PARAM_INPUT_2 = "state";

int Direction = 1;
int Rotation = 1;
bool Request = false;

const String FirmwareVer = {"1.9"};
#define URL_fw_Version "/Winggy-Wii/ESP8266_OTA_for_Final/main/version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/Winggy-Wii/ESP8266_OTA_for_Final/main/firmware.bin"
const char *host = "raw.githubusercontent.com";
const int httpsPort = 443;
bool lock = false;

IPAddress local_IP(192, 168, 1, 7);
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
DNSServer dns;

const char trustRoot[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
)EOF";
X509List cert(trustRoot);

extern const unsigned char caCert[] PROGMEM;
extern const unsigned int caCertLen;

void setClock()
{
  // Set time via NTP, as required for x.509 validation
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}

void FirmwareUpdate()
{
  WiFiClientSecure client;
  client.setTrustAnchors(&cert);
  if (!client.connect(host, httpsPort))
  {
    Serial.println("Connection failed");
    return;
  }
  client.print(String("GET ") + URL_fw_Version + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");
  while (client.connected())
  {
    String line = client.readStringUntil('\n');
    if (line == "\r")
    {
      //Serial.println("Headers received");
      break;
    }
  }
  String payload = client.readStringUntil('\n');

  payload.trim();
  if (payload.equals(FirmwareVer))
  {
    Serial.println("Device already on latest firmware version");
    lock = true;
  }
  else
  {
    lock = false;
    Serial.println("New firmware detected");
    ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, URL_fw_Bin);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
    }
  }
}
void connect_wifi();
unsigned long previousMillis_2 = 0;
unsigned long previousMillis = 0; // will store last time LED was updated
const long interval = 60000;
const long mini_interval = 1000;
void repeatedCall()
{
  unsigned long currentMillis = millis();
  if ((currentMillis - previousMillis) >= interval)
  {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    setClock();
    FirmwareUpdate();
  }

  if ((currentMillis - previousMillis_2) >= mini_interval)
  {
    static int idle_counter = 0;
    previousMillis_2 = currentMillis;
    Serial.print(" Active fw version:");
    Serial.println(FirmwareVer);
    Serial.print("Idle Loop....");
    Serial.println(idle_counter++);
    if (idle_counter % 2 == 0)
      digitalWrite(LED_BUILTIN, HIGH);
    else
      digitalWrite(LED_BUILTIN, LOW);
    if (WiFi.status() == !WL_CONNECTED)
      connect_wifi();
  }
}
void setup()
{
  Serial.begin(9600);

  

  pinMode(D3, INPUT_PULLUP);
  pinMode(D4, INPUT_PULLUP);
  my_Stepper.setSpeed(200);
  // Serial port for debugging purposes

  Serial.println("Start");
  WiFi.mode(WIFI_STA);
  connect_wifi();
  setClock();
  pinMode(LED_BUILTIN, OUTPUT);
  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial.println("STA Failed to configure");
  }

  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              String inputMessage1;
              String inputMessage2;
              // GET input1 value on <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
              if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2))
              {
                inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
                inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
                digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());
                Direction = inputMessage1.toInt();
                Rotation = inputMessage2.toInt();
                Request = true;
              }
              else
              {
                inputMessage1 = "No message sent";
                inputMessage2 = "No message sent";
              }
              Serial.print("GPIO: ");
              Serial.print(inputMessage1);
              Serial.print(" - Set to: ");
              Serial.println(inputMessage2);
              request->send(200, "text/plain", "OK");
            });

  // Start server
  server.begin();
}

void Stepper1(int Direction, int Rotation)
{ // function for stepper motor control with 2 parameters
  for (int i = 0; i < Rotation; i++)
  {                                   // for loop
    my_Stepper.step(Direction * 200); // 200 is 360 degree => change value if smaller then 360 degree is needing
  }
}

void loop()
{
  VL53L0X_RangingMeasurementData_t measure;

  Serial.println(PostitionSwitchA);
  Serial.println(PostitionSwitchB);
  PostitionSwitchB = digitalRead(D3);
  PostitionSwitchA = digitalRead(D4);
  DelayIndicator = millis();

  if (Initial)
  {
    if (measure.RangeStatus != 4)
    {

      if (measure.RangeMilliMeter < 350)
      {
        Serial.print("Distance (mm): ");
        Serial.println(measure.RangeMilliMeter);
        if (PostitionSwitchA == 0 || PostitionSwitchB == 0)
        {
          if (PostitionSwitchA == 0)
          {
            PrevSwitchA = true;
            PrevSwitchB = false;
          }
          else
          {
            PrevSwitchB = true;
            PrevSwitchA = false;
          }
        }
        if (PrevSwitchA)
        {
          Stepper1(1, 5);
        }
        if (PrevSwitchB)
        {
          Stepper1(-1, 5);
        }
        if (Request)
        {
          Stepper1(Direction, Rotation);
          PrevSwitchB = false;
          PrevSwitchA = false;
        }
      }
    }
    else
    {
      Serial.println(" out of range ");
    }
    delay(100);
  }
  else if (PostitionSwitchA == 0)
  {
    Initial = true;
    PrevSwitchA = true;
    PrevSwitchB = false;
  }
  else
  {
    Stepper1(-1, 1);
  }
}
void connect_wifi()
{
  AsyncWiFiManager wifiManager(&server, &dns);
  while (WiFi.status() != WL_CONNECTED)
  {
    wifiManager.autoConnect("WING's WiFi Manager");
  }
  Serial.println("Connected to WiFi");
}