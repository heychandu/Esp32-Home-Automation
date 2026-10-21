```cpp
/*
 * ESP32 Smart Home Automation
 * Author: Chandu R
 * GitHub: https://github.com/heychandu
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>

/* ==========================
   WiFi Credentials
   ========================== */

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

/* ==========================
   Pin Configuration
   ========================== */

constexpr uint8_t DEVICE_COUNT = 4;

const uint8_t OUTPUT_PINS[DEVICE_COUNT] = {
  16, 17, 18, 19
};

const uint8_t BUTTON_PINS[DEVICE_COUNT] = {
  12, 14, 26, 27
};

/* ==========================
   Device Structure
   ========================== */

struct Device {
  uint8_t outputPin;
  uint8_t buttonPin;
  bool state;
  bool previousButtonState;
};

Device devices[DEVICE_COUNT];

/* ==========================
   Web Server
   ========================== */

AsyncWebServer server(80);
AsyncEventSource events("/events");

/* ==========================
   HTML Page
   ========================== */

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Smart Home</title>

<style>
body{
font-family:Arial;
text-align:center;
background:#f5f5f5;
}

.button{
background:mediumseagreen;
color:white;
padding:12px 20px;
border:none;
font-size:22px;
cursor:pointer;
}

.button2{
background:gray;
color:white;
padding:12px 20px;
border:none;
font-size:22px;
cursor:pointer;
}

.reset{
background:crimson;
color:white;
padding:10px 20px;
border:none;
font-size:20px;
cursor:pointer;
}
</style>
</head>

<body>

<h1>ESP32 Smart Home Automation</h1>

<h3>Device 1</h3>
<p><a href="/set?id=0"><button id="btn1" class="%btn1class%">%btn1txt%</button></a></p>

<h3>Device 2</h3>
<p><a href="/set?id=1"><button id="btn2" class="%btn2class%">%btn2txt%</button></a></p>

<h3>Device 3</h3>
<p><a href="/set?id=2"><button id="btn3" class="%btn3class%">%btn3txt%</button></a></p>

<h3>Device 4</h3>
<p><a href="/set?id=3"><button id="btn4" class="%btn4class%">%btn4txt%</button></a></p>

<p>
<a href="/reset">
<button class="reset">Reset All</button>
</a>
</p>

<script>

if (!!window.EventSource) {

  var source = new EventSource('/events');

  source.addEventListener('toggleState', function(e) {

    let data = JSON.parse(e.data);

    let button = document.getElementById(data.id);

    if(data.status){
      button.innerHTML = "OFF";
      button.className = "button2";
    }
    else{
      button.innerHTML = "ON";
      button.className = "button";
    }

  }, false);
}
</script>

</body>
</html>
)rawliteral";

/* ==========================
   Template Processor
   ========================== */

String processor(const String& var)
{
  for (int i = 0; i < DEVICE_COUNT; i++)
  {
    if (var == "btn" + String(i + 1) + "txt")
      return devices[i].state ? "OFF" : "ON";

    if (var == "btn" + String(i + 1) + "class")
      return devices[i].state ? "button2" : "button";
  }

  return "";
}

/* ==========================
   Client Notification
   ========================== */

void notifyClients(uint8_t index)
{
  char json[64];

  snprintf(
    json,
    sizeof(json),
    "{\"id\":\"btn%d\",\"status\":%d}",
    index + 1,
    devices[index].state
  );

  events.send(json, "toggleState", millis());
}

/* ==========================
   Toggle Device
   ========================== */

void toggleDevice(uint8_t index)
{
  devices[index].state = !devices[index].state;

  digitalWrite(
    devices[index].outputPin,
    devices[index].state ? HIGH : LOW
  );

  notifyClients(index);
}

/* ==========================
   Reset All Devices
   ========================== */

void resetAllDevices()
{
  for (int i = 0; i < DEVICE_COUNT; i++)
  {
    if (devices[i].state)
    {
      toggleDevice(i);
    }
  }
}

/* ==========================
   Initialize Hardware
   ========================== */

void initializeDevices()
{
  for (int i = 0; i < DEVICE_COUNT; i++)
  {
    devices[i].outputPin = OUTPUT_PINS[i];
    devices[i].buttonPin = BUTTON_PINS[i];

    devices[i].state = false;
    devices[i].previousButtonState = HIGH;

    pinMode(devices[i].outputPin, OUTPUT);
    pinMode(devices[i].buttonPin, INPUT_PULLUP);

    digitalWrite(devices[i].outputPin, LOW);
  }
}

/* ==========================
   Setup
   ========================== */

void setup()
{
  Serial.begin(115200);

  initializeDevices();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp32"))
  {
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
      request->send_P(
        200,
        "text/html",
        index_html,
        processor
      );
    });

  server.on("/set", HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
      if (request->hasParam("id"))
      {
        int index =
          request->getParam("id")->value().toInt();

        if (index >= 0 && index < DEVICE_COUNT)
        {
          toggleDevice(index);
        }
      }

      request->send_P(
        200,
        "text/html",
        index_html,
        processor
      );
    });

  server.on("/reset", HTTP_GET,
    [](AsyncWebServerRequest *request)
    {
      resetAllDevices();

      request->send_P(
        200,
        "text/html",
        index_html,
        processor
      );
    });

  events.onConnect(
    [](AsyncEventSourceClient *client)
    {
      client->send(
        "Connected",
        NULL,
        millis(),
        1000
      );
    });

  server.addHandler(&events);

  server.begin();

  Serial.println("Server Started");
}

/* ==========================
   Loop
   ========================== */

void loop()
{
  for (int i = 0; i < DEVICE_COUNT; i++)
  {
    bool currentState =
      digitalRead(devices[i].buttonPin);

    if (devices[i].previousButtonState == HIGH &&
        currentState == LOW)
    {
      toggleDevice(i);

      delay(50); // Debounce
    }

    devices[i].previousButtonState =
      currentState;
  }
}
```
