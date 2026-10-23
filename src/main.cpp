#include <Arduino.h>

enum LedColor {
    RED = 0,
    GREEN = 1,
};

struct LED {
    LedColor color;
    uint8_t pinNumber;
    
};

constexpr uint8_t ADC_POT_PIN { 15 };
constexpr uint8_t BUTTON_PIN { 16 };

constexpr uint8_t GREEN_RGB_LED_PIN { 10 };
constexpr uint8_t RED_RGB_LED_PIN { 11 };
constexpr uint8_t BUZZER_PIN { 14 };

const int DIGIT_SEG[7] = {40, 39, 6, 5, 4, 41, 42};   // a b c d e f g

uint8_t lastButtonState { HIGH };
LedColor lastLedColor = { RED };
uint8_t lastLedState = { LOW };
uint8_t currentTargetNumber { 0 };
uint8_t countOfWrongInputs { 0 };

void showDigit(uint8_t digit);

void setup() {
    Serial.begin(115200);

    delay(500);

    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(GREEN_RGB_LED_PIN, OUTPUT);
    pinMode(RED_RGB_LED_PIN, OUTPUT);

    for (int i = 0; i < 7; ++i) {
        pinMode(DIGIT_SEG[i], OUTPUT);
    }

    currentTargetNumber = random(10);

    Serial.printf("Current number: %d \n", currentTargetNumber);
}

void loop() {
    uint16_t potRawValue = analogRead(ADC_POT_PIN);

    uint8_t potValue = map(potRawValue, 0, 4095, 9, 0);

    showDigit(potValue);

    uint8_t currentButtonState = digitalRead(BUTTON_PIN);

    if (currentButtonState != lastButtonState) {
        lastButtonState = currentButtonState;

        if (currentButtonState == LOW) {

            if (potValue == currentTargetNumber) {
                digitalWrite(GREEN_RGB_LED_PIN, HIGH);
                digitalWrite(RED_RGB_LED_PIN, LOW);

                tone(BUZZER_PIN, 1500, 100);

                delay(150);

                tone(BUZZER_PIN, 2500, 150);

                delay(150);

                countOfWrongInputs = 0;

                currentTargetNumber = random(10);

                Serial.printf("Current number: %d \n", currentTargetNumber);
            } else {
                digitalWrite(GREEN_RGB_LED_PIN, LOW);
                digitalWrite(RED_RGB_LED_PIN, HIGH);

                tone(BUZZER_PIN, 3000, 300);

                delay(300);

                countOfWrongInputs++;
            }

        }
    }

    if (countOfWrongInputs == 3) {
        Serial.println("Wrong input. Should alarm");

        for (int i = 0; i < 8; ++i) {
            tone(BUZZER_PIN, 400);

            delay(250);

            tone(BUZZER_PIN, 800);

            delay(250);
        }

        noTone(BUZZER_PIN);

        countOfWrongInputs = 0;

        currentTargetNumber = random(10);

        Serial.printf("Current number: %d \n", currentTargetNumber);
    }


    delay(100);
}

void showDigit(uint8_t digit) {
    uint8_t digitSegments[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};
    uint8_t pattern = digitSegments[digit];

    for (int i = 0; i < 7; ++i) {
        digitalWrite(DIGIT_SEG[i], pattern & (1 << i));
    }
}