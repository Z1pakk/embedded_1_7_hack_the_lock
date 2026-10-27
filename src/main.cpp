#include <Arduino.h>

namespace {

    // non-blocking timer: start it, then check isExpired() instead of calling delay()
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

        // subtraction keeps working when millis() overflows (after ~49 days)
        bool isExpired() const
        {
            return running && millis() - startTime >= interval;
        }
    };

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

    // RGB LED with common cathode: HIGH turns a color on
    struct RgbLed {
        uint8_t redPinNumber;
        uint8_t greenPinNumber;
        uint8_t bluePinNumber;

        LedState state { LedState::OFF };
        LedColor color { LedColor::NONE };

        explicit RgbLed(uint8_t redPinNumber, uint8_t greenPinNumber, uint8_t bluePinNumber) {
            this->redPinNumber = redPinNumber;
            this->greenPinNumber = greenPinNumber;
            this->bluePinNumber = bluePinNumber;
        };

        void begin() const {
            pinMode(redPinNumber, OUTPUT);
            pinMode(greenPinNumber, OUTPUT);
            pinMode(bluePinNumber, OUTPUT);
        }

        // lights only the chosen color, the other two are turned off
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

    // template of 7-segment for every digit from 0 to 9
    constexpr uint8_t SEVEN_SEGMENT_DIGIT_TEMPLATES[10] = {0b0111111, 0b0000110, 0b1011011, 0b1001111, 0b1100110, 0b1101101, 0b1111101, 0b0000111, 0b1111111, 0b1101111};

    // single 7-segment digit with common cathode
    struct SevenSegment {
        uint8_t pins[7]{};  // a b c d e f g
        uint8_t lastDigit = 0xFF;

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

        void showDigit(uint8_t digit) {
            if (digit > 9) {
                return;
            }

            if (digit == lastDigit) {
                return;
            }

            uint8_t pattern = SEVEN_SEGMENT_DIGIT_TEMPLATES[digit];

            // bit i of the pattern decides whether segment i is lit
            for (int i = 0; i < 7; ++i) {
                digitalWrite(pins[i], pattern & (1 << i));
            }

            this->lastDigit = digit;
        }
    };

    // push button to GND with the internal pull-up: released = HIGH, pressed = LOW
    struct Button {
        uint8_t pinNumber;
        uint8_t lastState = HIGH;
        bool pressedNow = false;

        Timer debounceTimer { 50 };

        explicit Button(uint8_t pinNumber) {
            this->pinNumber = pinNumber;
        }

        void begin() const {
            pinMode(pinNumber, INPUT_PULLUP);
        }

        // call on every loop pass, even when presses are ignored, so lastState never goes stale
        void update() {
            // a press only counts in the pass where it happened
            pressedNow = false;

            // contacts are still bouncing after the last change, ignore the pin
            if (debounceTimer.isRunning() && !debounceTimer.isExpired()) {
                return;
            }

            uint8_t currentState = digitalRead(pinNumber);

            if (currentState == lastState) {
                return;
            }

            lastState = currentState;
            debounceTimer.start();

            pressedNow = currentState == LOW;
        }

        // true only in the pass where the button went down, holding it does not repeat
        bool wasPressed() const {
            return pressedNow;
        }
    };

    constexpr uint8_t POT_NUM_SLICES { 10 };
    constexpr uint8_t POT_HYSTERESIS { 100 };
    constexpr uint16_t POT_ADC_BITS { 12 };
    constexpr uint16_t POT_ADC_MAX { (1 << POT_ADC_BITS) };
    constexpr uint8_t POT_SAMPLES { 8 };

    // knob that selects the digit
    struct Potentiometer {
        uint8_t pinNumber;
        uint8_t currentSlice { 0 };

        explicit Potentiometer(const uint8_t pinNumber) : pinNumber(pinNumber) {}

        void begin() const {
            pinMode(pinNumber, INPUT);
        }

        uint8_t readDigit()
        {
            uint16_t potRawValue = 0;

            // window filter
            for (uint8_t i = 0; i < POT_SAMPLES; ++i) {
                potRawValue += analogRead(pinNumber);
            }

            potRawValue /= POT_SAMPLES;

            // hysteresis filter
            uint16_t lowerBorder = currentSlice * POT_ADC_MAX / POT_NUM_SLICES;
            uint16_t upperBorder = (currentSlice + 1) * POT_ADC_MAX / POT_NUM_SLICES;

            if ((potRawValue + POT_HYSTERESIS) < lowerBorder || potRawValue > (upperBorder + POT_HYSTERESIS)) {
                currentSlice = static_cast<uint8_t>(potRawValue * POT_NUM_SLICES / POT_ADC_MAX);
            }

            // minus 1 to have max 9 slices always
            return POT_NUM_SLICES - currentSlice - 1;
        }
    };

    // one step of a sound: play this frequency for this long
    struct ToneStep {
        uint16_t frequency;  // 0 = silence
        uint16_t duration;   // ms
    };

    // correct guess, wrong guess and alarm (the alarm steps are repeated)
    constexpr ToneStep POSITIVE_SOUND[] = { {1500, 100}, {0, 50}, {2500, 150} };
    constexpr ToneStep NEGATIVE_SOUND[] = { {3000, 300} };
    constexpr ToneStep ALARM_SOUND[] = { {400, 250}, {800, 250} };

    // passive buzzer that plays sounds step by step without blocking the loop
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

        // cuts the sound off immediately
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

        // starts the tone of the current step and times how long it lasts
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

    // game logic: guess the secret digit, 3 wrong guesses in a row trigger the alarm
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

            // printed for debugging, so you can test without guessing
            Serial.printf("New secret code: %d \n", secretCode);
        }

        // called on every loop pass
        void update() {
            uint8_t guessedDigit = potentiometer.readDigit();

            // the display always follows the knob, even during the alarm
            sevenSegment.showDigit(guessedDigit);

            // keeps the current sound going
            buzzer.update();

            // always read the button, even during the alarm
            button.update();

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
                // correct: green light, happy sound, new secret
                if (secretCode == guessedDigit) {
                    buzzer.playPositive();
                    rgbLed.on(LedColor::GREEN);

                    secretCode = random(10);

                    Serial.printf("New secret code: %d \n", secretCode);

                    attempts = 0;
                }
                // wrong: red light, the third miss in a row starts the alarm
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

    // pin assignment, the wiring is shown in README.md
    constexpr uint8_t POT_ADC_PIN { 15 };
    constexpr uint8_t BUTTON_PIN { 16 };

    constexpr uint8_t RGB_GREEN_LED_PIN { 10 };
    constexpr uint8_t RGB_RED_LED_PIN { 11 };
    constexpr uint8_t RGB_BLUE_LED_PIN { 12 };

    constexpr uint8_t BUZZER_PIN { 14 };

    constexpr uint8_t DIGIT_SEG[7] = {40, 39, 6, 5, 4, 41, 42};   // a b c d e f g

    // hardware objects, the lock gets references to all of them
    RgbLed rgbLed { RGB_RED_LED_PIN, RGB_GREEN_LED_PIN, RGB_BLUE_LED_PIN };
    SevenSegment sevenSegment { DIGIT_SEG };
    Button button { BUTTON_PIN };
    Potentiometer potentiometer { POT_ADC_PIN };
    Buzzer buzzer { BUZZER_PIN };
    Lock lock { sevenSegment, potentiometer, rgbLed, button, buzzer };

}


void setup() {
    Serial.begin(115200);

    // give the Serial Monitor time to connect
    delay(500);

    // set up the pins first, then pick the first secret
    rgbLed.begin();
    sevenSegment.begin();
    button.begin();
    potentiometer.begin();
    buzzer.begin();
    lock.begin();
}

// no delay here: all timing is done with Timer
void loop() {
    lock.update();
}