#include <Arduino.h>

namespace {
    enum class LedState {
        OFF = 0,
        ON = 1,
    };

    enum class LedColor {
        NONE = 0,
        RED = 1,
        GREEN = 2,
        BLUE = 3,
    };

    struct Timer {
        unsigned long startTime = 0;
        unsigned long interval = 0;
        bool running = false;

        explicit Timer(unsigned long interval) : interval(interval) {}

        void start()
        {
            start(interval);
        }

        void start(const unsigned long ms)
        {
            startTime = millis();
            interval = ms;
            running = true;
        }

        void stop()
        {
            running = false;
        }

        bool isRunning() const
        {
            return this->running;
        }

        bool isExpired() const
        {
            return running && millis() - startTime >= interval;
        }
    };

    struct RgbLed {
        uint8_t greenPinNumber;
        uint8_t redPinNumber;
        uint8_t bluePinNumber;

        LedState state { LedState::OFF };
        LedColor color { LedColor::NONE };

        explicit RgbLed(uint8_t greenPinNumber, uint8_t redPinNumber, uint8_t bluePinNumber) {
            this->greenPinNumber = greenPinNumber;
            this->redPinNumber = redPinNumber;
            this->bluePinNumber = bluePinNumber;
        };

        void begin() const {
            pinMode(greenPinNumber, OUTPUT);
            pinMode(redPinNumber, OUTPUT);
            pinMode(bluePinNumber, OUTPUT);
        }

        void on(LedColor newColor)
        {
            state = LedState::ON;
            this->color = newColor;

            switch (newColor) {
                case LedColor::RED: {
                    digitalWrite(redPinNumber, HIGH);
                    digitalWrite(greenPinNumber, LOW);
                    digitalWrite(bluePinNumber, LOW);
                    break;
                }
                case LedColor::GREEN: {
                    digitalWrite(redPinNumber, LOW);
                    digitalWrite(greenPinNumber, HIGH);
                    digitalWrite(bluePinNumber, LOW);
                    break;
                }
                case LedColor::BLUE: {
                    digitalWrite(redPinNumber, LOW);
                    digitalWrite(greenPinNumber, LOW);
                    digitalWrite(bluePinNumber, HIGH);
                    break;
                }
                default:
                    off();
            }
        }

        void off()
        {
            state = LedState::OFF;
            color = LedColor::NONE;

            digitalWrite(redPinNumber, LOW);
            digitalWrite(greenPinNumber, LOW);
            digitalWrite(bluePinNumber, LOW);
        }
    };

    struct SevenSegment {
        uint8_t pins[7]{};  // a b c d e f g

        uint8_t digitSegments[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

        explicit SevenSegment(const uint8_t pins[7]) {
            for (int i = 0; i < 7; ++i) {
                this->pins[i] = pins[i];
            }
        };

        void begin() const {
            for (int i = 0; i < 7; ++i) {
                pinMode(pins[i], OUTPUT);
            }
        }

        void showDigit(uint8_t digit) const {
            if (digit > 9) {
                return;
            }

            uint8_t pattern = digitSegments[digit];

            for (int i = 0; i < 7; ++i) {
                digitalWrite(pins[i], pattern & (1 << i));
            }

        }
    };

    struct Button {
        uint8_t pinNumber;
        uint8_t lastState = HIGH;

        Timer debounceTimer { 50 };

        explicit Button(uint8_t pinNumber) {
            this->pinNumber = pinNumber;
        }

        void begin() const {
            pinMode(pinNumber, INPUT_PULLUP);
        }

        bool wasPressed() {
            // contacts are still bouncing after the last change, ignore the pin
            if (debounceTimer.isRunning() && !debounceTimer.isExpired()) {
                return false;
            }

            uint8_t currentState = digitalRead(pinNumber);

            if (currentState == lastState) {
                return false;
            }

            lastState = currentState;
            debounceTimer.start();

            return currentState == LOW;
        }
    };

    struct Potentiometer {
        uint8_t pinNumber;

        void begin() const {
            pinMode(pinNumber, INPUT);
        }

        uint8_t readDigit() const
        {
            uint16_t potRawValue = analogRead(pinNumber);

            return map(potRawValue, 0, 4095, 9, 0);
        }
    };

    struct ToneStep {
        uint16_t frequency;  // 0 = silence
        uint16_t duration;   // ms
    };

    constexpr ToneStep POSITIVE_SOUND[] = { {1500, 100}, {0, 50}, {2500, 150} };
    constexpr ToneStep NEGATIVE_SOUND[] = { {3000, 300} };
    constexpr ToneStep ALARM_SOUND[] = { {400, 250}, {800, 250} };

    struct Buzzer {
        uint8_t pinNumber;

        const ToneStep* steps = nullptr;
        uint8_t stepCount = 0;
        uint8_t currentStep = 0;
        uint8_t repeatsLeft = 0;

        Timer stepTimer { 0 };

        explicit Buzzer(uint8_t pinNumber) {
            this->pinNumber = pinNumber;
        }

        void begin() const {
            pinMode(pinNumber, OUTPUT);
        }

        void playPositive() {
            play(POSITIVE_SOUND);
        }

        void playNegative() {
            play(NEGATIVE_SOUND);
        }

        void playAlarm() {
            play(ALARM_SOUND, 8);
        }

        // N is the number of steps, the compiler takes it from the array size
        template <size_t N>
        void play(const ToneStep (&sound)[N], uint8_t repeats = 1)
        {
            steps = sound;
            stepCount = N;
            currentStep = 0;
            repeatsLeft = repeats;

            playCurrentStep();
        }

        void stop()
        {
            noTone(pinNumber);
            stepTimer.stop();
            steps = nullptr;
        }

        bool isPlaying() const
        {
            return stepTimer.isRunning();
        }

        // call on every loop pass, moves to the next step when the current one is over
        void update()
        {
            if (!stepTimer.isExpired()) {
                return;
            }

            ++currentStep;

            if (currentStep >= stepCount) {
                if (repeatsLeft <= 1) {
                    stop();
                    return;
                }

                --repeatsLeft;
                currentStep = 0;
            }

            playCurrentStep();
        }

        void playCurrentStep()
        {
            const ToneStep& step = steps[currentStep];

            if (step.frequency == 0) {
                noTone(pinNumber);
            }
            else {
                tone(pinNumber, step.frequency);
            }

            stepTimer.start(step.duration);
        }
    };

    struct Lock {
        uint8_t secretCode = 0;
        uint8_t attempts = 0;
        static constexpr uint8_t maxAttempts = 3;
        bool alarmActive = false;

        SevenSegment& sevenSegment;
        Potentiometer& potentiometer;
        RgbLed& rgbLed;
        Button& button;
        Buzzer& buzzer;

        explicit Lock(SevenSegment& sevenSegment, Potentiometer& potentiometer, RgbLed& rgbLed, Button& button, Buzzer& buzzer) : sevenSegment(sevenSegment),
            potentiometer(potentiometer),
            rgbLed(rgbLed),
            button(button),
            buzzer(buzzer)
        {
        }

        void begin()
        {
            secretCode = random(10);

            Serial.printf("New secret code: %d \n", secretCode);
        }

        void update() {
            uint8_t guessedDigit = potentiometer.readDigit();

            sevenSegment.showDigit(guessedDigit);

            buzzer.update();

            // lockout: ignore the button until the alarm has finished
            if (alarmActive) {
                if (buzzer.isPlaying()) {
                    return;
                }

                alarmActive = false;
                attempts = 0;

                secretCode = random(10);

                Serial.printf("New secret code: %d \n", secretCode);

                rgbLed.off();
            }

            if (button.wasPressed()) {
                if (secretCode == guessedDigit) {
                    buzzer.playPositive();
                    rgbLed.on(LedColor::GREEN);

                    secretCode = random(10);

                    Serial.printf("New secret code: %d \n", secretCode);

                    attempts = 0;
                }
                else {
                    ++attempts;

                    rgbLed.on(LedColor::RED);

                    if (attempts >= maxAttempts) {
                        buzzer.playAlarm();
                        alarmActive = true;
                    }
                    else {
                        buzzer.playNegative();
                    }
                }
            }
        }
    };

    constexpr uint8_t POT_ADC_PIN { 15 };
    constexpr uint8_t BUTTON_PIN { 16 };

    constexpr uint8_t RGB_GREEN_LED_PIN { 10 };
    constexpr uint8_t RGB_RED_LED_PIN { 11 };
    constexpr uint8_t RGB_BLUE_LED_PIN { 12 };

    constexpr uint8_t BUZZER_PIN { 14 };

    constexpr uint8_t DIGIT_SEG[7] = {40, 39, 6, 5, 4, 41, 42};   // a b c d e f g

    RgbLed rgbLed { RGB_GREEN_LED_PIN, RGB_RED_LED_PIN, RGB_BLUE_LED_PIN };
    SevenSegment sevenSegment { DIGIT_SEG };
    Button button { BUTTON_PIN };
    Potentiometer potentiometer { POT_ADC_PIN };
    Buzzer buzzer { BUZZER_PIN };
    Lock lock { sevenSegment, potentiometer, rgbLed, button, buzzer };

}


void setup() {
    Serial.begin(115200);

    delay(500);

    rgbLed.begin();
    sevenSegment.begin();
    button.begin();
    potentiometer.begin();
    buzzer.begin();
    lock.begin();
}

void loop() {
    lock.update();
}