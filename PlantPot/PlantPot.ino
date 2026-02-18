#include <Arduino.h>
#include "esp_sleep.h"

#include <WiFi.h>
#include <AsyncTCP.h>
#include "esp_sleep.h"
#include <ESPAsyncWebServer.h>

#define BUTTON_PIN 4 // RTC-capable pin

const int pumpPin = 5; 

bool pumpRunning = false;
unsigned long pumpStopTime = 0;

const unsigned long TIMER_24H = 24UL * 60ULL * 60ULL * 1000000ULL;
// const unsigned long TIMER_24H = 15ULL * 1000000ULL;

RTC_DATA_ATTR double waterAmount = 0;
RTC_DATA_ATTR double maxWaterAmount = 0;
RTC_DATA_ATTR double days = 0;
RTC_DATA_ATTR double currentDay = 0;
RTC_DATA_ATTR bool seedSettingActive = false;
RTC_DATA_ATTR bool timer24hActive = false;

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Water Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin:0 auto; padding-top: 20px;}
    input {
      padding: 10px;
      font-size: 18px;
      width: 220px;
      margin-bottom: 15px;
    }
    .button {
      padding: 10px 20px;
      font-size: 20px;
      color: #fff;
      background-color: #2f4468;
      border: none;
      border-radius: 5px;
      cursor: pointer;
      margin: 5px;
    }
    .button:hover { background-color: #1f2e45; }
    .hidden { display: none; }
  </style>
</head>
<body>
  <h1>Water Dispenser</h1>

  <p>
    Timer status:
    <strong id="timerStatus" style="color:red;">STOPPED</strong>
  </p>

  <button class="button" onclick="togglePage()">Toggle Mode</button>

  <!-- PAGE 1 -->
<div id="pageSingle">
  <h2>Single Amount</h2>
  <p>Water amount</p>
  <input type="number" id="amount" min="1" value="%MIN_WATER%">
  <br>
  <button class="button" onclick="sendAmount()">Confirm</button>
  <button class="button" onclick="cancel()">Cancel</button>
</div>

<!-- PAGE 2 -->
<div id="pageMulti" class="hidden">
  <h2>Multi-Day Schedule</h2>

  <p>Minimum water amount</p>
  <input type="number" id="amount1" min="1" value="%MIN_WATER%"><br>
  <p>Maximum water amount</p>
  <input type="number" id="amount2" min="1" value="%MAX_WATER%"><br>
  <p>Amount of days</p>
  <input type="number" id="days" min="1" value="%DAYS%"><br>

  <button class="button" onclick="sendMulti()">Confirm</button>
  <button class="button" onclick="cancel()">Cancel</button>

  <div id="seedStatus" style="margin-bottom:15px;">
    <p>
      Day:
      <strong id="currentDay">0</strong> /
      <strong id="totalDays">0</strong>
    </p>
    <p>
      Next watering:
      <strong id="nextWater">0</strong> ml
    </p>
  </div>

</div>

<script>
let showingSingle = true;

function sendAmount() {
  var amount = document.getElementById("amount").value;
  if (amount === "") {
    alert("Please enter an amount");
    return;
  }
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/water?amount=" + amount, true);
  xhr.onload = updateStatus;
  xhr.send();
}

function sendMulti() {
  var a1 = document.getElementById("amount1").value;
  var a2 = document.getElementById("amount2").value;
  var d  = document.getElementById("days").value;

  if (a1 === "" || a2 === "" || d === "") {
    alert("Fill all fields");
    return;
  }

  var xhr = new XMLHttpRequest();
  xhr.open("GET",
    "/water_multi?amount1=" + a1 + "&amount2=" + a2 + "&days=" + d,
    true
  );
  xhr.onload = updateStatus;
  updateSeedStatus();
  xhr.send();
}

function cancel() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/cancel", true);
  xhr.onload = updateStatus;
  updateSeedStatus();
  xhr.send();
}


function updateStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/status", true);
  xhr.onload = function () {
    if (xhr.status === 200) {
      var data = JSON.parse(xhr.responseText);
      var el = document.getElementById("timerStatus");
      el.textContent = data.running ? "RUNNING" : "STOPPED";
      el.style.color = data.running ? "green" : "red";
    }
  };
  xhr.send();
}

function updateSeedStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/seed_status", true);
  xhr.onload = function () {
    if (xhr.status === 200) {
      var data = JSON.parse(xhr.responseText);

      if (!data.active) return;

      document.getElementById("currentDay").textContent = data.currentDay;
      document.getElementById("totalDays").textContent = data.days;
      document.getElementById("nextWater").textContent = data.nextWater;
    }
  };
  xhr.send();
}

window.onload = function () {
  updateStatus();
  updateSeedStatus();
};

function togglePage() {
  showingSingle = !showingSingle;
  document.getElementById("pageSingle").classList.toggle("hidden");
  document.getElementById("pageMulti").classList.toggle("hidden");

  if (!showingSingle) {
    updateSeedStatus();
  }
}

</script>
</body>
</html>
)rawliteral";

String templateProcessor(const String& var) {
  if (var == "MIN_WATER") {
    return waterAmount > 0 ? String(waterAmount, 0) : "0";
  }
  if (var == "MAX_WATER") {
    return maxWaterAmount > 0 ? String(maxWaterAmount, 0) : "0";
  }
  if (var == "DAYS") {
    return days > 0 ? String(days, 0) : "0";
  }
  if (var == "TIMER_STATUS") {
    return timer24hActive ? "RUNNING" : "STOPPED";
  }
  if (var == "TIMER_COLOR") {
    return timer24hActive ? "green" : "red";
  }
  return "";
}

void goToDeepSleep() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  // Timer wakeup
  if(timer24hActive == true){
    esp_sleep_enable_timer_wakeup(TIMER_24H);
  }

  // specify which pin wakes the esp
  uint64_t mask = 1ULL << BUTTON_PIN;
  // esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_HIGH);

  // enter deep sleep
  digitalWrite(pumpPin, LOW); // just in case
  Serial.println("Entering deep sleep...");
  delay(100);
  esp_deep_sleep_start();
}



void runWaterFunction(int amountML) {
  Serial.print("Water requested: ");
  Serial.print(amountML);
  Serial.println(" mL");

  // set water amount so that the pump can be repeated by the timer
  waterAmount = amountML;
  // calculate amount of time to run the pump 
  unsigned long runTime = amountML/22.0  * 1000;

  // set pump duration + extra time for water to flow through pipe
  pumpStopTime = millis() + runTime + 500;
  pumpRunning = true;
  digitalWrite(pumpPin, HIGH);

  // 80,000 * (t * 60 * 60) = 500 mililitrers a hour
  // 22ml a second
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

AsyncWebServer server(80);

void enableWebserver(){
  WiFi.mode(WIFI_AP);
  WiFi.softAP("MyESP32_AP", "12345678");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("Connect to Wi-Fi at: http://");
  Serial.println(IP);
  
  // Send web page to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, templateProcessor);
  });

  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("amount")) {

      // water
      int amount = request->getParam("amount")->value().toInt();
      // waterAmount = amount;
      
      // begin water timer
      timer24hActive = true;

      // ensure seed setting is no longer active
      seedSettingActive = false;
      
      request->send(200, "text/plain", "Water amount received");

      runWaterFunction(amount);

    } else {
      request->send(400, "text/plain", "Missing amount");
    }
  });

  server.on("/water_multi", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("amount1") && request->hasParam("amount2") && request->hasParam("days")) {

      // store varaibles
      waterAmount = request->getParam("amount1")->value().toInt();
      maxWaterAmount = request->getParam("amount2")->value().toInt();
      days = request->getParam("days")->value().toInt();
      currentDay = 0;
      
      // begin water timer
      timer24hActive = true;

      // ensure seed setting is active
      seedSettingActive = true;
      
      request->send(200, "text/plain", "Water amount received");
      
      runWaterFunction(waterAmount);

    } else {
      request->send(400, "text/plain", "Missing amount");
    }
  });

  server.on("/cancel", HTTP_GET, [](AsyncWebServerRequest *request) {

    // cancel timer and pump
    timer24hActive = false;
    pumpStopTime = 0;
    pumpRunning = false;
    digitalWrite(pumpPin, LOW);

    request->send(200, "text/plain", "Cancelled");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{ \"running\": ";
    json += timer24hActive ? "true" : "false";
    json += " }";
    request->send(200, "application/json", json);
  });

  server.on("/seed_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{";
    json += "\"active\":"; json += seedSettingActive ? "true" : "false";
    json += ",\"currentDay\":"; json += String(currentDay, 0);
    json += ",\"days\":"; json += String(days, 0);
    json += ",\"nextWater\":"; json += String(waterAmount, 0);
    json += "}";

    request->send(200, "application/json", json);
  });
  
  server.onNotFound(notFound);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000); // wait for Serial Monitor to connect on startup

  // enable pins
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button to GND
  // pinMode(BUTTON_PIN, INPUT); // Button to GND
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Woke up from button press!");
    enableWebserver();
    Serial.println("Webserver running");
  } 
  else if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    // water plants
    if(seedSettingActive == false){
      runWaterFunction(waterAmount);
    }
    else{
      if(currentDay < days){
        currentDay++;

        Serial.print("day: ");
        Serial.print(currentDay);
        Serial.println("");

        // exponential calculation
        // R = (B/A)^(1/T) - 1
        // B = future value (max water)
        // A = original value (min water)
        // R = growth rate ()
        // T = time period (days)
        double growthRate = pow((maxWaterAmount/waterAmount),(currentDay/days)) - 1;

        Serial.print("growth rate: ");
        Serial.print(growthRate);
        Serial.println("");

        waterAmount = waterAmount + (waterAmount * growthRate);

        Serial.print("water amount: ");
        Serial.print(waterAmount);
        Serial.println("");
      }

      runWaterFunction(waterAmount);
    }
    
    // go back to sleep
    // goToDeepSleep();
  }
  else {

    // if the switch is on while batteries are plugged in then keep website on
    if (digitalRead(BUTTON_PIN) == LOW) {
      Serial.println("Power-on or reset. Waiting 3 sec before sleep...");
      delay(3000); // <-- Give time to open Serial Monitor
      goToDeepSleep();
    }
    else if (digitalRead(BUTTON_PIN) == HIGH){
      enableWebserver();
      Serial.println("Webserver running");
    }

    
  }

  Serial.println("Device awake. Press button to sleep again.");
}

void loop() {
  // stop pumping water once timer is up
  if (pumpRunning && millis() >= pumpStopTime){
    digitalWrite(pumpPin, LOW);
    pumpRunning = false;
    Serial.println("Pump stopped");

    if(digitalRead(BUTTON_PIN) == LOW){
      goToDeepSleep();
    }
  }
  
  // go to sleep once button pressed
  if (digitalRead(BUTTON_PIN) == LOW && pumpRunning == false) {
    goToDeepSleep();
  }

}
