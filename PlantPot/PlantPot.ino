#include <Arduino.h>
#include "esp_sleep.h"

#define BUTTON_PIN 4 // RTC-capable pin

RTC_DATA_ATTR int bootCount = 0; 

void goToDeepSleep() {
  Serial.println("Going to deep sleep...");
  delay(50);

  uint64_t mask = 1ULL << BUTTON_PIN;
  esp_deep_sleep_enable_gpio_wakeup(mask, ESP_GPIO_WAKEUP_GPIO_LOW);

  Serial.println("Entering deep sleep now...");
  delay(100);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000); // <-- Wait for Serial Monitor to connect

  pinMode(BUTTON_PIN, INPUT_PULLUP); // Button to GND

  esp_sleep_wakeup_cause_t reason = esp_sleep_get_wakeup_cause();
  if (reason == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Woke up from button press!");

    bootCount++; // This value persists across deep sleep
    Serial.println("Boot number: " + String(bootCount));
  } else {
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
    goToDeepSleep();
  }

  delay(500);
}
