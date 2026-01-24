/*********
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp32-esp8266-web-server-outputs-momentary-switch/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*********/

#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

// REPLACE WITH YOUR NETWORK CREDENTIALS
const char* ssid = ""; 
const char* password = ""; 

const int output = 2;
unsigned long waterAmount = 0;

bool timer24hActive = false;
unsigned long timer24hStart = 0;

// const unsigned long TIMER_24H = 24UL * 60UL * 60UL * 1000UL;
const unsigned long TIMER_24H = 10000;

bool pumpRunning = false;
unsigned long pumpStopTime = 0;

const int pumpPin = 2; 

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Water Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin:0px auto; padding-top: 30px;}
    input {
      padding: 10px;
      font-size: 20px;
      width: 200px;
      margin-bottom: 20px;
    }
    .button {
      padding: 10px 20px;
      font-size: 24px;
      color: #fff;
      background-color: #2f4468;
      border: none;
      border-radius: 5px;
      cursor: pointer;
    }
    .button:hover { background-color: #1f2e45; }
  </style>
</head>

<body>
  <h1>Water Dispenser</h1>

  <input type="number" id="amount" placeholder="mL of water" min="1">
  <br><br>
  <button class="button" onclick="sendAmount()">Confirm</button>
  <button class="button" onclick="cancel()">Cancel</button>

  <h2 id="timer">No timer running</h2>

  <script>
  function sendAmount() {
    var amount = document.getElementById("amount").value;
    if (amount === "") {
      alert("Please enter an amount");
      return;
    }

    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/water?amount=" + amount, true);
    xhr.send();
  }

  function cancel() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/cancel", true);
    xhr.send();
  }

  // Poll ESP every second
  setInterval(function() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/time", true);
    xhr.onload = function() {
      var seconds = parseInt(this.responseText);
      if (seconds <= 0) {
        document.getElementById("timer").innerHTML = "No timer running";
      } else {
        document.getElementById("timer").innerHTML = formatTime(seconds);
      }
    };
    xhr.send();
  }, 1000);

  function formatTime(sec) {
    var h = Math.floor(sec / 3600);
    var m = Math.floor((sec % 3600) / 60);
    var s = sec % 60;
    return "Time remaining: " +
          String(h).padStart(2, '0') + ":" +
          String(m).padStart(2, '0') + ":" +
          String(s).padStart(2, '0');
  }
  </script>
</body>
</html>
)rawliteral";

void runWaterFunction(int amountML);

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Failed!");
    return;
  }
  Serial.println();
  Serial.print("ESP IP Address: http://");
  Serial.println(WiFi.localIP());
  
  pinMode(output, OUTPUT);
  digitalWrite(output, LOW);
  
  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });

  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("amount")) {

      // water
      int amount = request->getParam("amount")->value().toInt();
      runWaterFunction(amount);

      // timer
      if (timer24hActive) {
        request->send(403, "text/plain", "Timer already running");
        return;
      }
      
      // begin water timer
      timer24hStart = millis();
      timer24hActive = true;
      
      request->send(200, "text/plain", "Water amount received + timer started");

    } else {
      request->send(400, "text/plain", "Missing amount");
    }
  });

  server.on("/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (timer24hActive == false) {
      request->send(200, "text/plain", "0");
      return;
    }

    // calculate time since timer was started
    unsigned long elapsed = millis() - timer24hStart;
    unsigned long remaining = (elapsed >= TIMER_24H) ? 0 : (TIMER_24H - elapsed);

    request->send(200, "text/plain", String(remaining / 1000));
  });

  server.on("/cancel", HTTP_GET, [](AsyncWebServerRequest *request) {

    // cancel timer and pump
    timer24hActive = false;
    timer24hStart = 0;
    pumpStopTime = 0;

    pumpRunning = false;
    digitalWrite(pumpPin, LOW);

    request->send(200, "text/plain", "Cancelled");
  });
  
  server.onNotFound(notFound);
  server.begin();
}

void runWaterFunction(int amountML) {
  Serial.print("Water requested: ");
  Serial.print(amountML);
  Serial.println(" mL");

  // set water amount so that the pump can be repeated by the timer
  waterAmount = amountML;
  // calculate amount of time to run the pump 
  unsigned long runTime = amountML/22  * 1000;

  digitalWrite(2, HIGH);
  pumpStopTime = millis() + runTime;
  pumpRunning = true;

  // 80,000 * (t * 60 * 60) = 500 mililitrers a hour
  // 22ml a second
}

void loop() {
  // stop pump after amount of water is pumped
  if (pumpRunning && millis() >= pumpStopTime) {
    digitalWrite(pumpPin, LOW);
    pumpRunning = false;
    Serial.println("Pump stopped");
  }

  // pump water once timer has expired and repeat timer
  if (timer24hActive && millis() - timer24hStart >= TIMER_24H) {
    
    unsigned long runTime = waterAmount/22  * 1000;
    digitalWrite(2, HIGH);
    
    // repeat timer
    pumpStopTime = millis() + runTime;
    pumpRunning = true;
    timer24hStart = millis();
  }
}
