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

const unsigned long TIMER_24H = 24UL * 60UL * 60UL * 1000UL;
// const unsigned long TIMER_24H = 10000;

const int uS_TO_S_FACTOR = 1000000ULL;
RTC_DATA_ATTR uint64_t TIME_TO_SLEEP = 60;
RTC_DATA_ATTR bool CONFIGURED = false;

bool timer24hActive = false;
unsigned long timer24hStart = 0;

RTC_DATA_ATTR int waterAmount = 0;

// HTML web page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP Water Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; text-align: center; margin:0 auto; padding-top: 30px;}
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

  <button class="button" onclick="togglePage()">Toggle Mode</button>

  <!-- PAGE 1 -->
  <div id="pageSingle">
    <h2>Single Amount</h2>
    <input type="number" id="amount" placeholder="mL of water" min="1">
    <br>
    <button class="button" onclick="sendAmount()">Confirm</button>
    <button class="button" onclick="cancel()">Cancel</button>
  </div>

  <!-- PAGE 2 -->
  <div id="pageMulti" class="hidden">
    <h2>Multi-Day Schedule</h2>

    <input type="number" id="amount1" placeholder="Amount 1 (mL)" min="1"><br>
    <input type="number" id="amount2" placeholder="Amount 2 (mL)" min="1"><br>
    <input type="number" id="days" placeholder="Number of days" min="1"><br>

    <button class="button" onclick="sendMulti()">Confirm</button>
    <button class="button" onclick="cancel()">Cancel</button>
  </div>

  <h2 id="timer">No timer running</h2>

<script>
let showingSingle = true;

function togglePage() {
  showingSingle = !showingSingle;
  document.getElementById("pageSingle").classList.toggle("hidden");
  document.getElementById("pageMulti").classList.toggle("hidden");
}

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
  xhr.send();
}

function cancel() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/cancel", true);
  xhr.send();
}

// Poll timer
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
         String(h).padStart(2,'0') + ":" +
         String(m).padStart(2,'0') + ":" +
         String(s).padStart(2,'0');
}
</script>
</body>
</html>
)rawliteral";

// used on the first start
void goToDeepSleep() {
  Serial.println("Going to deep sleep...");
  delay(50);

  uint64_t mask = 1ULL << BUTTON_PIN;
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);

  Serial.println("Entering deep sleep now...");
  delay(100);
  esp_deep_sleep_start();
}

// used for all other purposes
void prepareSleep(uint64_t sleepUs) {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  // Timer wakeup
  esp_sleep_enable_timer_wakeup(sleepUs);

  // GPIO wakeup (deep sleep compatible)
  uint64_t mask = 1ULL << BUTTON_PIN;
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);

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
  unsigned long runTime = amountML/22  * 1000;

  Serial.println("2");
  pumpStopTime = millis() + runTime;
  pumpRunning = true;
  digitalWrite(pumpPin, HIGH);

  while (pumpRunning){
    if (pumpRunning && millis() >= pumpStopTime){
      digitalWrite(pumpPin, LOW);
      pumpRunning = false;
      Serial.println("Pump stopped");
    }
  }

  // SET SLEEP TIMER
  prepareSleep(TIME_TO_SLEEP * 1000000UL);

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
    request->send(200, "text/html", index_html);
  });

  server.on("/water", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("amount")) {

      // water
      int amount = request->getParam("amount")->value().toInt();
      // waterAmount = amount;

      // timer
      if (timer24hActive) {
        request->send(403, "text/plain", "Timer already running");
        return;
      }
      
      // begin water timer
      timer24hStart = millis();
      timer24hActive = true;
      
      request->send(200, "text/plain", "Water amount received + timer started");
      CONFIGURED = true;
      runWaterFunction(amount);

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

int64_t remaining = TIME_TO_SLEEP * uS_TO_S_FACTOR;
uint64_t now = 0;

void setup() {
  Serial.begin(115200);
  delay(1000); // <-- Wait for Serial Monitor to connect

  uint64_t now = esp_timer_get_time();

  // enable pins
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button to GND
  pinMode(pumpPin, OUTPUT);
  digitalWrite(pumpPin, LOW);

  

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_GPIO) {
    if (CONFIGURED == true){
      remaining = (TIME_TO_SLEEP - now) / 1000000;
      Serial.printf("Manual wake! Remaining time was: %lld seconds\n", remaining);
    }
    
    Serial.println("Woke up from button press!");
    enableWebserver();
    Serial.println("Webserver running");
  } 
  else if (reason == ESP_SLEEP_WAKEUP_TIMER) {
    // water plants
    runWaterFunction(waterAmount);
    // // go back to sleep
    // esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * 1000000UL);
    // esp_deep_sleep_start();
  }
  else {
    Serial.println("Power-on or reset. Waiting 3 sec before sleep...");
    delay(3000); // <-- Give time to open Serial Monitor
    goToDeepSleep();
  }
}

void loop() {
  Serial.println("Device awake. Press button to sleep again.");
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(200); // debounce
    while (digitalRead(BUTTON_PIN) == LOW) delay(10); // wait for release

    if(CONFIGURED == true){
      uint64_t sleepUs = TIME_TO_SLEEP * 1000000ULL;
      // uint64_t now targetTime = now + sleepUs; 
      prepareSleep(remaining * 1000000UL - now);
    }
    else{
      goToDeepSleep();
    }

    
  }

  delay(500);
}
