#include <Arduino.h>
#include "LedControl.h"
#include "binary.h"
#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>

const uint8_t LCD_PRIMARY_ADDRESS_START = 0x20;
const uint8_t LCD_PRIMARY_ADDRESS_END = 0x27;
const uint8_t LCD_SECONDARY_ADDRESS_START = 0x38;
const uint8_t LCD_SECONDARY_ADDRESS_END = 0x3F;
const uint8_t LCD_FALLBACK_ADDRESS = 0x27;
const int LCD_COLUMNS = 16;
const int LCD_ROWS = 2;

const int MATRIX_DIN_PIN = 7;
const int MATRIX_CS_PIN = 9;
const int MATRIX_CLK_PIN = 8;
const int MATRIX_TOTAL_ROWS = 8;

const int TOTAL_TM1637_DISPLAYS = 5;
const int TOTAL_ANSWER_DISPLAYS = 3;

const int EASY_NUMBER_LIMIT = 10;
const int MEDIUM_NUMBER_LIMIT = 50;
const int HARD_NUMBER_LIMIT = 100;
const int EASY_DECIMAL_ROUND_CHANCE_PERCENT = 10;
const int MEDIUM_DECIMAL_ROUND_CHANCE_PERCENT = 20;
const int HARD_DECIMAL_ROUND_CHANCE_PERCENT = 30;

const int EASY_MULTIPLICATION_LIMIT = 5;
const int MEDIUM_MULTIPLICATION_LIMIT = 12;
const int HARD_MULTIPLICATION_LIMIT = 20;

const int EASY_DIVISOR_LIMIT = 5;
const int MEDIUM_DIVISOR_LIMIT = 12;
const int HARD_DIVISOR_LIMIT = 20;

const int EASY_DIVIDEND_LIMIT = 25;
const int MEDIUM_DIVIDEND_LIMIT = 144;
const int HARD_DIVIDEND_LIMIT = 400;

const int EASY_POTENTIOMETER_LIMIT = 80;
const int MEDIUM_POTENTIOMETER_LIMIT = 160;
const int LEVEL_HYSTERESIS = 8;

const int MAX_WRONG_ANSWER_OFFSET = 10;
const int DISPLAY_BRIGHTNESS = 0xF;
const int MATRIX_BRIGHTNESS = 7;
const uint8_t EMPTY_DISPLAY_SEGMENTS[4] = {0x0, 0x0, 0x0, 0x0};

const int DISPLAY_CLK_PINS[TOTAL_TM1637_DISPLAYS] = {2, 3, 4, 5, 6};
const int DISPLAY_DIO_PIN = 10;
const int ANSWER_BUTTON_PINS[] = {A1, A2, A3};
const int RESET_BUTTON_PIN = 11;
const int CORRECT_LED_PIN = 12;
const int INCORRECT_LED_PIN = 13;
const int POTENTIOMETER_PIN = A0;

const int TOTAL_BUTTONS = 4;
const int PRESSED_BUTTON_LEVEL = LOW;

const unsigned long DEBOUNCE_MS = 40;
const unsigned long INCORRECT_FEEDBACK_MS = 1000;
const unsigned long ANSWER_FLASH_MS = 500;

const int CORRECT_ANSWER_FLASH_CYCLES = 5;
const int OPERATION_FLASH_STEPS = 4;

enum Operation { OP_NONE, OP_ADD, OP_SUBTRACT, OP_MULTIPLY, OP_DIVIDE };
enum DifficultyLevel { LEVEL_EASY = 1, LEVEL_MEDIUM, LEVEL_HARD };
enum GameStatus {
  STATUS_SELECTING_OPERATION,
  STATUS_WAITING_FOR_ANSWER,
  STATUS_SHOWING_CORRECT_ANSWER,
  STATUS_SHOWING_INCORRECT_ANSWER
};

struct GameState {
  int correct_answer_position = 0;
  bool round_started = false;
  double operand_1 = 0;
  double operand_2 = 0;
  double operation_result = 0;
  Operation current_operation = OP_NONE;
  int level = 0;
  int correct_answers = 0;
  int incorrect_answers = 0;
  GameStatus status = STATUS_WAITING_FOR_ANSWER;
  bool previous_button_readings[TOTAL_BUTTONS] = {HIGH, HIGH, HIGH, HIGH};
  bool stable_button_states[TOTAL_BUTTONS] = {HIGH, HIGH, HIGH, HIGH};
  unsigned long last_debounce_time[TOTAL_BUTTONS] = {0, 0, 0, 0};
  unsigned long feedback_start_time = 0;
  unsigned long last_flash_toggle_time = 0;
  unsigned long last_operation_animation_time = 0;
  int flashes_remaining = 0;
  bool answer_visible = false;
  TM1637Display* correct_answer_display = NULL;
  int total_operation_animation_steps = 0;
  int current_operation_animation_step = 0;
  int operation_flash_stage = -1;
  int result_decimal_places = 0;
};

struct LCDState {
  bool initialised = false;
  GameStatus status = STATUS_WAITING_FOR_ANSWER;
  int level = LEVEL_EASY;
  int correct_answers = 0;
  int incorrect_answers = 0;
  Operation operation = OP_NONE;
};

LiquidCrystal_I2C* lcd = NULL;
LedControl led_matrix = LedControl(MATRIX_DIN_PIN, MATRIX_CS_PIN, MATRIX_CLK_PIN, 1);

TM1637Display number_display_1(DISPLAY_CLK_PINS[0], DISPLAY_DIO_PIN);
TM1637Display number_display_2(DISPLAY_CLK_PINS[1], DISPLAY_DIO_PIN);
TM1637Display option_display_1(DISPLAY_CLK_PINS[2], DISPLAY_DIO_PIN);
TM1637Display option_display_2(DISPLAY_CLK_PINS[3], DISPLAY_DIO_PIN);
TM1637Display option_display_3(DISPLAY_CLK_PINS[4], DISPLAY_DIO_PIN);

TM1637Display* tm1637_displays[] = {
  &number_display_1,
  &number_display_2,
  &option_display_1,
  &option_display_2,
  &option_display_3
};

int reset_button_pin = RESET_BUTTON_PIN;
int correct_led_pin = CORRECT_LED_PIN;
int incorrect_led_pin = INCORRECT_LED_PIN;
int answer_button_pins[] = {ANSWER_BUTTON_PINS[0], ANSWER_BUTTON_PINS[1], ANSWER_BUTTON_PINS[2]};
int potentiometer_pin = POTENTIOMETER_PIN;

GameState game;
LCDState lcd_state;

byte blank_symbol[] = {
  B00000000, B00000000, B00000000, B00000000,
  B00000000, B00000000, B00000000, B00000000
};
byte add_symbol[] = {
  B00000000, B00011000, B00011000, B01111110,
  B01111110, B00011000, B00011000, B00000000
};
byte subtract_symbol[] = {
  B00000000, B00011000, B00011000, B00011000,
  B00011000, B00011000, B00011000, B00000000
};
byte multiply_symbol[] = {
  B00000000, B01000010, B00100100, B00011000,
  B00011000, B00100100, B01000010, B00000000
};
byte divide_symbol[] = {
  B00000000, B00011000, B00011000, B01011010,
  B01011010, B00011000, B00011000, B00000000
};

int getRequiredDecimalPlacesForExactDivision(int dividend, int divisor);
int getMaxDisplayDecimalPlaces(double value);
double roundToDecimalPlaces(double value, int decimal_places);
int getMinimumDisplayDecimalPlaces(double value);
void showNumber(TM1637Display &display, double value, int forced_decimal_places);
void showNumber(TM1637Display &display, double value);

bool isI2cDeviceAvailable(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

uint8_t findLcdAddress() {
  for (uint8_t address = LCD_PRIMARY_ADDRESS_START; address <= LCD_PRIMARY_ADDRESS_END; address++) {
    if (isI2cDeviceAvailable(address)) {
      return address;
    }
  }

  for (uint8_t address = LCD_SECONDARY_ADDRESS_START; address <= LCD_SECONDARY_ADDRESS_END; address++) {
    if (isI2cDeviceAvailable(address)) {
      return address;
    }
  }

  return LCD_FALLBACK_ADDRESS;
}

void initialiseLcd() {
  Wire.begin();
  uint8_t lcd_address = findLcdAddress();
  lcd = new LiquidCrystal_I2C(lcd_address, LCD_COLUMNS, LCD_ROWS);

  lcd->init();
  lcd->setBacklight(HIGH);
  Serial.print(F("LCD I2C address: 0x"));
  if (lcd_address < 0x10) {
    Serial.print('0');
  }
  Serial.println(lcd_address, HEX);
}

// ---------------------------------------------------------------------------
// Initialisation and main loop.
// ---------------------------------------------------------------------------
void setup() {
  led_matrix.shutdown(0, false);
  randomSeed(analogRead(0));
  led_matrix.setIntensity(0, MATRIX_BRIGHTNESS);
  led_matrix.clearDisplay(0);
  resetAllDisplays();

  Serial.begin(9600);
  pinMode(correct_led_pin, OUTPUT);
  pinMode(incorrect_led_pin, OUTPUT);

  for (int i = 0; i < TOTAL_ANSWER_DISPLAYS; i++) {
    // Buttons use the internal pull-up resistor, so pressed reads LOW.
    pinMode(answer_button_pins[i], INPUT_PULLUP);
  }

  pinMode(reset_button_pin, INPUT_PULLUP);
  pinMode(potentiometer_pin, INPUT);

  initialiseLcd();
}

// Main game loop.
void loop() {
  if (!game.round_started) {
    startNewRound();
    game.round_started = true;
  }

  updateAnswerFeedback();

  if (buttonWasPressed(3, reset_button_pin)) {
    game.correct_answers = 0;
    game.incorrect_answers = 0;
    restartSystem();
    showLcdData();
    return;
  }

  if (game.status == STATUS_SELECTING_OPERATION) {
    updateOperationAnimation();
  }

  if (game.status == STATUS_WAITING_FOR_ANSWER) {
    handleAnswerButtons();
  }
}

// ---------------------------------------------------------------------------
// Round state control.
// ---------------------------------------------------------------------------
void showCorrectAnswer(TM1637Display &display) {
  digitalWrite(correct_led_pin, HIGH);
  digitalWrite(incorrect_led_pin, LOW);
  game.correct_answer_display = &display;
  game.answer_visible = false;
  game.flashes_remaining = CORRECT_ANSWER_FLASH_CYCLES * 2;
  game.last_flash_toggle_time = millis();
  game.feedback_start_time = millis();
  game.status = STATUS_SHOWING_CORRECT_ANSWER;
  showLcdData();
}

void restartSystem() {
  resetAllDisplays();
  showOperationSymbol(OP_NONE);
  digitalWrite(correct_led_pin, LOW);
  digitalWrite(incorrect_led_pin, LOW);

  game.correct_answer_position = 0;
  game.answer_visible = false;
  game.flashes_remaining = 0;
  game.correct_answer_display = NULL;
  game.operand_1 = 0;
  game.operand_2 = 0;
  game.total_operation_animation_steps = 0;
  game.current_operation_animation_step = 0;
  game.operation_flash_stage = -1;
  game.result_decimal_places = 0;
  game.status = STATUS_SELECTING_OPERATION;
  game.round_started = false;
}

void resetAllDisplays() {
  for (int i = 0; i < TOTAL_TM1637_DISPLAYS; i++) {
    resetDisplay(*tm1637_displays[i]);
  }
}

void clearLcdLine(int row) {
  lcd->setCursor(0, row);
  lcd->print(F("                "));
}

bool lcdNeedsUpdate() {
  if (!lcd_state.initialised) return true;
  if (lcd_state.status != game.status) return true;
  if (lcd_state.level != game.level) return true;
  if (lcd_state.correct_answers != game.correct_answers) return true;
  if (lcd_state.incorrect_answers != game.incorrect_answers) return true;
  if (lcd_state.operation != game.current_operation) return true;
  return false;
}

void saveLcdState() {
  lcd_state.initialised = true;
  lcd_state.status = game.status;
  lcd_state.level = game.level;
  lcd_state.correct_answers = game.correct_answers;
  lcd_state.incorrect_answers = game.incorrect_answers;
  lcd_state.operation = game.current_operation;
}

// ---------------------------------------------------------------------------
// Difficulty rules and random selection.
// ---------------------------------------------------------------------------
int getNumberLimitForLevel() {
  if (game.level == LEVEL_EASY) return EASY_NUMBER_LIMIT;
  if (game.level == LEVEL_MEDIUM) return MEDIUM_NUMBER_LIMIT;
  return HARD_NUMBER_LIMIT;
}

int getMultiplicationLimitForLevel() {
  if (game.level == LEVEL_EASY) return EASY_MULTIPLICATION_LIMIT;
  if (game.level == LEVEL_MEDIUM) return MEDIUM_MULTIPLICATION_LIMIT;
  return HARD_MULTIPLICATION_LIMIT;
}

int getDivisorLimitForLevel() {
  if (game.level == LEVEL_EASY) return EASY_DIVISOR_LIMIT;
  if (game.level == LEVEL_MEDIUM) return MEDIUM_DIVISOR_LIMIT;
  return HARD_DIVISOR_LIMIT;
}

int getDividendLimitForLevel() {
  if (game.level == LEVEL_EASY) return EASY_DIVIDEND_LIMIT;
  if (game.level == LEVEL_MEDIUM) return MEDIUM_DIVIDEND_LIMIT;
  return HARD_DIVIDEND_LIMIT;
}

int clampInt(int value, int minimum, int maximum) {
  if (value < minimum) return minimum;
  if (value > maximum) return maximum;
  return value;
}

int getWrongAnswerIntegerOffset() {
  if (game.current_operation == OP_ADD || game.current_operation == OP_SUBTRACT) {
    return clampInt((int)round(game.operand_2 / 2.0), 1, MAX_WRONG_ANSWER_OFFSET);
  }

  if (game.current_operation == OP_MULTIPLY) {
    return clampInt((int)round(max(game.operand_1, game.operand_2) / 2.0), 2, 15);
  }

  return clampInt((int)round(game.operand_2), 1, MAX_WRONG_ANSWER_OFFSET);
}

double getWrongAnswerStep(int decimal_places) {
  if (decimal_places == 0) {
    return (double)getWrongAnswerIntegerOffset();
  }

  double step = 1.0;
  for (int i = 0; i < decimal_places; i++) {
    step /= 10.0;
  }

  int maximum_multiplier = game.current_operation == OP_DIVIDE ? 4 : 6;
  return step * random(1, maximum_multiplier + 1);
}

bool shouldGenerateExactDivision() {
  return true;
}

bool shouldUseDecimalRound() {
  if (game.level == LEVEL_EASY) {
    return random(0, 100) < EASY_DECIMAL_ROUND_CHANCE_PERCENT;
  }
  if (game.level == LEVEL_MEDIUM) {
    return random(0, 100) < MEDIUM_DECIMAL_ROUND_CHANCE_PERCENT;
  }
  return random(0, 100) < HARD_DECIMAL_ROUND_CHANCE_PERCENT;
}

double buildDecimalOperandFromInt(int base_value) {
  int decimal_places = random(1, 3);  // 1 or 2 decimal places.
  int scale = decimal_places == 1 ? 10 : 100;
  int decimal_part = random(1, scale);
  return (double)base_value + ((double)decimal_part / (double)scale);
}

int getGreatestCommonDivisor(int first_value, int second_value) {
  first_value = abs(first_value);
  second_value = abs(second_value);

  while (second_value != 0) {
    int remainder = first_value % second_value;
    first_value = second_value;
    second_value = remainder;
  }

  return first_value == 0 ? 1 : first_value;
}

int getRequiredDecimalPlacesForExactDivision(int dividend, int divisor) {
  if (divisor == 0) return -1;

  int gcd = getGreatestCommonDivisor(dividend, divisor);
  int reduced_divisor = abs(divisor) / gcd;
  int power_of_two = 0;
  int power_of_five = 0;

  while (reduced_divisor % 2 == 0) {
    reduced_divisor /= 2;
    power_of_two++;
  }

  while (reduced_divisor % 5 == 0) {
    reduced_divisor /= 5;
    power_of_five++;
  }

  if (reduced_divisor != 1) {
    return -1;
  }

  return max(power_of_two, power_of_five);
}

int getMaxDisplayDecimalPlaces(double value) {
  long integer_part = (long)value;
  if (integer_part < 0) integer_part = -integer_part;

  int integer_digits = 1;
  while (integer_part >= 10) {
    integer_part /= 10;
    integer_digits++;
  }

  if (integer_digits >= 4) return 0;
  if (integer_digits == 3) return 1;
  if (integer_digits == 2) return 2;
  return 3;
}

bool divisionHasAtMostThreeDecimalPlaces(int dividend, int divisor) {
  int required_decimal_places = getRequiredDecimalPlacesForExactDivision(dividend, divisor);
  if (required_decimal_places < 0 || required_decimal_places > 3) {
    return false;
  }

  double result = (double)dividend / (double)divisor;
  int max_display_decimal_places = getMaxDisplayDecimalPlaces(result);
  return required_decimal_places <= max_display_decimal_places;
}

int getMinimumDisplayDecimalPlaces(double value) {
  for (int decimal_places = 0; decimal_places <= 3; decimal_places++) {
    double rounded_value = roundToDecimalPlaces(value, decimal_places);
    if (fabs(value - rounded_value) < 0.0005) {
      return decimal_places;
    }
  }

  return 3;
}

double getDisplayMaximumForDecimalPlaces(int decimal_places) {
  int scale = 1;
  for (int i = 0; i < decimal_places; i++) {
    scale *= 10;
  }
  return 9999.0 / (double)scale;
}

double drawNearbyWrongAnswer(double correct_answer, int decimal_places, int preferred_direction) {
  int scale = getDecimalScale(decimal_places);
  double step = 1.0 / (double)scale;
  double maximum_value = getDisplayMaximumForDecimalPlaces(decimal_places);

  double minimum_distance = decimal_places == 0 ? 1.0 : step * 2.0;
  if (game.level == LEVEL_EASY) minimum_distance = decimal_places == 0 ? 2.0 : step * 3.0;

  double distance_cap = 40.0;
  if (game.level == LEVEL_EASY) distance_cap = 120.0;
  else if (game.level == LEVEL_MEDIUM) distance_cap = 70.0;
  if (decimal_places > 0) distance_cap *= 0.6;

  double adaptive_distance = fabs(correct_answer) * 0.40 + (game.level == LEVEL_EASY ? 8.0 : 4.0);
  double maximum_distance = min(distance_cap, max(minimum_distance * 2.0, adaptive_distance));

  int minimum_steps = max(1, (int)ceil(minimum_distance * scale));
  int maximum_steps = max(minimum_steps + 1, (int)floor(maximum_distance * scale));
  int offset_steps = random(minimum_steps, maximum_steps + 1);

  int direction = preferred_direction;
  if (direction == 0) direction = random(0, 2) == 0 ? -1 : 1;

  double candidate = correct_answer + (direction * offset_steps * step);
  if (candidate < 0.0 || candidate > maximum_value) {
    candidate = correct_answer - (direction * offset_steps * step);
  }

  candidate = roundToDecimalPlaces(candidate, decimal_places);
  if (candidate < 0.0) candidate = 0.0;
  if (candidate > maximum_value) candidate = maximum_value;

  if (candidate == correct_answer) {
    double fallback = correct_answer + ((direction > 0 ? -1.0 : 1.0) * minimum_steps * step);
    candidate = roundToDecimalPlaces(fallback, decimal_places);
    if (candidate < 0.0) candidate = roundToDecimalPlaces(correct_answer + (minimum_steps * step), decimal_places);
  }

  return candidate;
}

const char* getOperationSymbolText(Operation operation) {
  if (operation == OP_ADD) return "+";
  if (operation == OP_SUBTRACT) return "-";
  if (operation == OP_MULTIPLY) return "x";
  if (operation == OP_DIVIDE) return "/";
  return "?";
}

String formatValueForSerial(double value, int decimal_places) {
  int places = decimal_places;
  if (places < 0) {
    places = getMinimumDisplayDecimalPlaces(value);
  }

  double rounded_value = roundToDecimalPlaces(value, places);
  if (places == 0) {
    return String((long)round(rounded_value));
  }

  return String(rounded_value, places);
}

void printRoundDebug(double option_1, double option_2, double option_3, int result_decimal_places) {
  int operand_1_places = getMinimumDisplayDecimalPlaces(game.operand_1);
  int operand_2_places = getMinimumDisplayDecimalPlaces(game.operand_2);

  Serial.println(F("---- Round ----"));
  Serial.print(F("Equation: "));
  Serial.print(formatValueForSerial(game.operand_1, operand_1_places));
  Serial.print(' ');
  Serial.print(getOperationSymbolText(game.current_operation));
  Serial.print(' ');
  Serial.print(formatValueForSerial(game.operand_2, operand_2_places));
  Serial.print(F(" = "));
  Serial.println(formatValueForSerial(game.operation_result, result_decimal_places));

  Serial.print(F("Options: [1] "));
  Serial.print(formatValueForSerial(option_1, result_decimal_places));
  Serial.print(F(" | [2] "));
  Serial.print(formatValueForSerial(option_2, result_decimal_places));
  Serial.print(F(" | [3] "));
  Serial.println(formatValueForSerial(option_3, result_decimal_places));

  Serial.print(F("Correct option: "));
  Serial.println(game.correct_answer_position);
}

void drawOperands(int &first_draw, int &second_draw) {
  int number_limit = getNumberLimitForLevel();

  if (game.current_operation == OP_ADD) {
    first_draw = random(1, number_limit + 1);
    second_draw = random(1, number_limit + 1);
    return;
  }

  if (game.current_operation == OP_SUBTRACT) {
    first_draw = random(1, number_limit + 1);
    second_draw = random(1, first_draw + 1);
    return;
  }

  if (game.current_operation == OP_MULTIPLY) {
    int multiplication_limit = getMultiplicationLimitForLevel();
    first_draw = random(1, multiplication_limit + 1);
    second_draw = random(1, multiplication_limit + 1);
    return;
  }

  int dividend_limit = getDividendLimitForLevel();
  bool prefer_exact_division = shouldGenerateExactDivision();

  for (int attempts = 0; attempts < 40; attempts++) {
    int divisor = random(2, getDivisorLimitForLevel() + 1);

    if (prefer_exact_division) {
      int maximum_quotient = max(1, dividend_limit / divisor);
      second_draw = divisor;
      first_draw = divisor * random(1, maximum_quotient + 1);
    } else {
      first_draw = random(1, dividend_limit + 1);
      second_draw = divisor;
    }

    if (divisionHasAtMostThreeDecimalPlaces(first_draw, second_draw)) {
      return;
    }
  }

  int safe_divisor = random(2, getDivisorLimitForLevel() + 1);
  int maximum_quotient = max(1, dividend_limit / safe_divisor);
  second_draw = safe_divisor;
  first_draw = safe_divisor * random(1, maximum_quotient + 1);
}

const char* getLevelName() {
  if (game.level == LEVEL_EASY) return "Easy";
  if (game.level == LEVEL_MEDIUM) return "Medium";
  return "Hard";
}

const char* getShortOperationName() {
  if (game.current_operation == OP_ADD) return "Add";
  if (game.current_operation == OP_SUBTRACT) return "Sub";
  if (game.current_operation == OP_MULTIPLY) return "Mult";
  if (game.current_operation == OP_DIVIDE) return "Div";
  return "----";
}

Operation drawOperationForLevel() {
  int weight = random(0, 100);

  if (game.level == LEVEL_EASY) {
    if (weight < 40) return OP_ADD;
    if (weight < 75) return OP_SUBTRACT;
    if (weight < 92) return OP_MULTIPLY;
    return OP_DIVIDE;
  }

  if (game.level == LEVEL_MEDIUM) {
    if (weight < 30) return OP_ADD;
    if (weight < 55) return OP_SUBTRACT;
    if (weight < 78) return OP_MULTIPLY;
    return OP_DIVIDE;
  }

  if (weight < 20) return OP_ADD;
  if (weight < 40) return OP_SUBTRACT;
  if (weight < 70) return OP_MULTIPLY;
  return OP_DIVIDE;
}

int getLevelFromPotentiometer(int reading) {
  if (game.level <= LEVEL_EASY) {
    if (reading >= MEDIUM_POTENTIOMETER_LIMIT + LEVEL_HYSTERESIS) return LEVEL_HARD;
    if (reading >= EASY_POTENTIOMETER_LIMIT + LEVEL_HYSTERESIS) return LEVEL_MEDIUM;
    return LEVEL_EASY;
  }

  if (game.level == LEVEL_MEDIUM) {
    if (reading <= EASY_POTENTIOMETER_LIMIT - LEVEL_HYSTERESIS) return LEVEL_EASY;
    if (reading >= MEDIUM_POTENTIOMETER_LIMIT + LEVEL_HYSTERESIS) return LEVEL_HARD;
    return LEVEL_MEDIUM;
  }

  if (reading <= EASY_POTENTIOMETER_LIMIT - LEVEL_HYSTERESIS) return LEVEL_EASY;
  if (reading <= MEDIUM_POTENTIOMETER_LIMIT - LEVEL_HYSTERESIS) return LEVEL_MEDIUM;
  return LEVEL_HARD;
}

void startNewRound() {
  readPotentiometer();
  game.total_operation_animation_steps = random(10, 15);
  game.current_operation_animation_step = 0;
  game.operation_flash_stage = -1;
  game.current_operation = OP_NONE;
  showOperationSymbol(OP_NONE);
  game.last_operation_animation_time = millis();
  game.status = STATUS_SELECTING_OPERATION;
  showLcdData();
}

void finishNewRound() {
  generateNumbers();
  shuffleDisplayPositions();
  game.status = STATUS_WAITING_FOR_ANSWER;
  showLcdData();
}

// ---------------------------------------------------------------------------
// Answer input and feedback.
// ---------------------------------------------------------------------------
void handleCorrectAnswer() {
  Serial.println(F("correct answer"));
  game.correct_answers++;
  showCorrectAnswer(*tm1637_displays[game.correct_answer_position + 1]);
}

void handleIncorrectAnswer() {
  Serial.println(F("incorrect answer"));
  digitalWrite(correct_led_pin, LOW);
  digitalWrite(incorrect_led_pin, HIGH);
  game.incorrect_answers++;
  game.feedback_start_time = millis();
  game.status = STATUS_SHOWING_INCORRECT_ANSWER;
  showLcdData();
}

void handleAnswerButtons() {
  for (int i = 0; i < TOTAL_ANSWER_DISPLAYS; i++) {
    if (buttonWasPressed(i, answer_button_pins[i])) {
      if (game.correct_answer_position == i + 1) {
        handleCorrectAnswer();
      } else {
        handleIncorrectAnswer();
      }
      return;
    }
  }
}

bool buttonWasPressed(int index, int pin) {
  bool reading = digitalRead(pin);

  if (reading != game.previous_button_readings[index]) {
    game.last_debounce_time[index] = millis();
    game.previous_button_readings[index] = reading;
  }

  if ((millis() - game.last_debounce_time[index]) > DEBOUNCE_MS &&
      reading != game.stable_button_states[index]) {
    game.stable_button_states[index] = reading;
    if (readingMeansButtonPressed(game.stable_button_states[index])) {
      return true;
    }
  }

  return false;
}

bool readingMeansButtonPressed(bool reading) {
  return reading == PRESSED_BUTTON_LEVEL;
}

void showLcdSelecting() {
  clearLcdLine(0);
  clearLcdLine(1);
  lcd->setCursor(0, 0);
  lcd->print(F("Mode: "));
  lcd->print(getLevelName());
  lcd->setCursor(0, 1);
  lcd->print(F("Selecting..."));
}

void showLcdAnswerResult(bool is_correct) {
  clearLcdLine(0);
  clearLcdLine(1);
  lcd->setCursor(0, 0);
  lcd->print(is_correct ? F("Right Answer") : F("Wrong Answer"));
  lcd->setCursor(0, 1);
  lcd->print(F("Right:"));
  lcd->print(game.correct_answers);
  lcd->print(F(" Wrong:"));
  lcd->print(game.incorrect_answers);
}

void showLcdGame() {
  clearLcdLine(0);
  clearLcdLine(1);
  lcd->setCursor(0, 0);
  lcd->print(getLevelName());
  lcd->print(" ");
  lcd->print(getShortOperationName());
  lcd->setCursor(0, 1);
  lcd->print(F("Right:"));
  lcd->print(game.correct_answers);
  lcd->print(F(" Wrong:"));
  lcd->print(game.incorrect_answers);
}

// ---------------------------------------------------------------------------
// Visual feedback and operation animation.
// ---------------------------------------------------------------------------
void updateAnswerFeedback() {
  if (game.status == STATUS_SHOWING_INCORRECT_ANSWER) {
    if (millis() - game.feedback_start_time >= INCORRECT_FEEDBACK_MS) {
      digitalWrite(correct_led_pin, LOW);
      digitalWrite(incorrect_led_pin, LOW);
      game.status = STATUS_WAITING_FOR_ANSWER;
      showLcdData();
    }
    return;
  }

  if (game.status == STATUS_SHOWING_CORRECT_ANSWER) {
    if (millis() - game.last_flash_toggle_time >= ANSWER_FLASH_MS) {
      game.last_flash_toggle_time = millis();

      if (game.answer_visible) {
        resetDisplay(*game.correct_answer_display);
      } else {
        showNumber(*game.correct_answer_display, game.operation_result, game.result_decimal_places);
      }

      game.answer_visible = !game.answer_visible;
      game.flashes_remaining--;
    }

    if (game.flashes_remaining <= 0) {
      restartSystem();
    }
  }
}

// Visual operation selection.
void updateOperationAnimation() {
  if (game.operation_flash_stage < 0) {
    unsigned long interval = 100 + (game.current_operation_animation_step * 10);
    if (millis() - game.last_operation_animation_time >= interval) {
      game.current_operation = drawOperationForLevel();
      showOperationSymbol(game.current_operation);
      game.last_operation_animation_time = millis();
      game.current_operation_animation_step++;

      if (game.current_operation_animation_step >= game.total_operation_animation_steps) {
        game.operation_flash_stage = 0;
        showOperationSymbol(OP_NONE);
        game.last_operation_animation_time = millis();
      }
    }
    return;
  }

  unsigned long interval = (game.operation_flash_stage % 2 == 0) ? 500 : 250;
  if (millis() - game.last_operation_animation_time < interval) {
    return;
  }

  game.operation_flash_stage++;
  game.last_operation_animation_time = millis();

  if (game.operation_flash_stage >= OPERATION_FLASH_STEPS) {
    finishNewRound();
    return;
  }

  if (game.operation_flash_stage % 2 == 0) {
    showOperationSymbol(OP_NONE);
  } else {
    showOperationSymbol(game.current_operation);
  }
}

// ---------------------------------------------------------------------------
// Number and answer generation.
// ---------------------------------------------------------------------------
double performOperation(double number_1, double number_2, Operation operation) {
  if (operation == OP_ADD) {
    return number_1 + number_2;
  } else if (operation == OP_SUBTRACT) {
    return number_1 - number_2;
  } else if (operation == OP_MULTIPLY) {
    return number_1 * number_2;
  } else if (operation == OP_DIVIDE) {
    return number_2 != 0 ? (double)number_1 / (double)number_2 : 0;
  }

  return 0;
}

void generateNumbers() {
  int first_draw = 0;
  int second_draw = 0;
  bool decimal_round = false;

  do {
    drawOperands(first_draw, second_draw);
    double first_value = (double)first_draw;
    double second_value = (double)second_draw;
    decimal_round = shouldUseDecimalRound();

    if (decimal_round) {
      if (game.current_operation == OP_ADD || game.current_operation == OP_SUBTRACT) {
        bool first_is_decimal = random(0, 2) == 0;
        bool second_is_decimal = random(0, 2) == 0;
        if (!first_is_decimal && !second_is_decimal) {
          first_is_decimal = true;
        }

        if (first_is_decimal) first_value = buildDecimalOperandFromInt(first_draw);
        if (second_is_decimal) second_value = buildDecimalOperandFromInt(second_draw);
      } else if (game.current_operation == OP_MULTIPLY) {
        if (random(0, 2) == 0) first_value = buildDecimalOperandFromInt(first_draw);
        else second_value = buildDecimalOperandFromInt(second_draw);
      } else if (game.current_operation == OP_DIVIDE) {
        int scale = random(0, 2) == 0 ? 10 : 100;
        bool found_decimal_division = false;

        for (int attempts = 0; attempts < 12; attempts++) {
          int scaled_dividend = first_draw * scale + random(1, scale);
          int required_decimal_places = getRequiredDecimalPlacesForExactDivision(scaled_dividend, second_draw * scale);
          if (required_decimal_places > 0 && required_decimal_places <= 3) {
            first_value = (double)scaled_dividend / (double)scale;
            found_decimal_division = true;
            break;
          }
        }

        if (!found_decimal_division) {
          decimal_round = false;
        }
      } else {
        if (random(0, 2) == 0) first_value = buildDecimalOperandFromInt(first_draw);
        else second_value = buildDecimalOperandFromInt(second_draw);
      }
    }

    game.operand_1 = first_value;
    game.operand_2 = second_value;
    game.operation_result = performOperation(first_value, second_value, game.current_operation);
  } while (game.operation_result < 0 || game.operation_result >= 10000);

  game.result_decimal_places = 0;
  if (decimal_round) {
    int minimum_decimal_places = getMinimumDisplayDecimalPlaces(game.operation_result);
    int max_display_decimal_places = getMaxDisplayDecimalPlaces(game.operation_result);
    if (minimum_decimal_places > 0 && max_display_decimal_places >= minimum_decimal_places) {
      game.result_decimal_places = random(minimum_decimal_places, max_display_decimal_places + 1);
    }
  } else if (game.current_operation == OP_DIVIDE) {
    int required_decimal_places = getRequiredDecimalPlacesForExactDivision((int)round(game.operand_1), (int)round(game.operand_2));
    int max_display_decimal_places = getMaxDisplayDecimalPlaces(game.operation_result);

    if (required_decimal_places > 0 && max_display_decimal_places >= required_decimal_places) {
      game.result_decimal_places = random(required_decimal_places, max_display_decimal_places + 1);
    }
  }

  showNumber(number_display_1, game.operand_1);
  showNumber(number_display_2, game.operand_2);
  Serial.println(String(F("operation result: ")) + String(game.operation_result));
}

void shuffleDisplayPositions() {
  game.correct_answer_position = random(1, 4);
  double answers[3] = {game.operation_result, 0, 0};
  int decimal_places = game.result_decimal_places;
  answers[1] = drawNearbyWrongAnswer(game.operation_result, decimal_places, -1);
  answers[2] = drawNearbyWrongAnswer(game.operation_result, decimal_places, 1);

  for (int attempts = 0; attempts < 10 && (answers[1] == game.operation_result || answers[2] == game.operation_result || answers[1] == answers[2]); attempts++) {
    answers[1] = drawNearbyWrongAnswer(game.operation_result, decimal_places, 0);
    answers[2] = drawNearbyWrongAnswer(game.operation_result, decimal_places, 0);
  }

  if (game.correct_answer_position == 2) {
    double temporary = answers[0];
    answers[0] = answers[1];
    answers[1] = temporary;
  } else if (game.correct_answer_position == 3) {
    double temporary = answers[0];
    answers[0] = answers[2];
    answers[2] = temporary;
  }

  printRoundDebug(answers[0], answers[1], answers[2], decimal_places);

  for (int i = 0; i < TOTAL_ANSWER_DISPLAYS; i++) {
    showNumber(*tm1637_displays[i + 2], answers[i], decimal_places);
  }
}

// ---------------------------------------------------------------------------
// Potentiometer reading and display utilities.
// ---------------------------------------------------------------------------
void readPotentiometer() {
  int potentiometer_reading = analogRead(potentiometer_pin);
  potentiometer_reading = map(potentiometer_reading, 0, 1023, 0, 255);

  if (game.level < LEVEL_EASY || game.level > LEVEL_HARD) {
    game.level = LEVEL_EASY;
  }

  game.level = getLevelFromPotentiometer(potentiometer_reading);
}

void resetDisplay(TM1637Display &display) {
  display.setBrightness(DISPLAY_BRIGHTNESS);
  display.setSegments(EMPTY_DISPLAY_SEGMENTS);
}

int countDigits(int value) {
  if (value == 0) return 1;

  int counter = 0;
  while (value > 0) {
    value /= 10;
    counter++;
  }

  return counter;
}

int getDecimalPlaces(double value) {
  long integer_part = (long)value;
  double decimal_part = value - (double)integer_part;

  if (fabs(decimal_part) < 0.0005) {
    return 0;
  }

  int integer_digits = countDigits(integer_part);
  if (integer_digits >= 4) return 0;
  if (integer_digits == 3) return 1;
  if (integer_digits == 2) return 2;
  return 3;
}

double roundToDecimalPlaces(double value, int decimal_places) {
  double factor = 1.0;
  for (int i = 0; i < decimal_places; i++) {
    factor *= 10.0;
  }
  return round(value * factor) / factor;
}

int getDecimalScale(int decimal_places) {
  int scale = 1;
  for (int i = 0; i < decimal_places; i++) {
    scale *= 10;
  }
  return scale;
}

uint8_t getDecimalDot(int decimal_places) {
  if (decimal_places == 1) return 0b00100000;
  if (decimal_places == 2) return 0b01000000;
  return 0b10000000;
}

void showNumber(TM1637Display &display, double value, int forced_decimal_places) {
  if (value < 0 || value >= 10000) {
    resetDisplay(display);
    return;
  }

  int decimal_places = forced_decimal_places;
  if (decimal_places < 0) {
    decimal_places = getMinimumDisplayDecimalPlaces(value);
  }

  int max_display_decimal_places = getMaxDisplayDecimalPlaces(value);
  decimal_places = clampInt(decimal_places, 0, max_display_decimal_places);
  double rounded_value = roundToDecimalPlaces(value, decimal_places);

  if (decimal_places == 0) {
    display.showNumberDec((int)round(rounded_value), false);
    return;
  }

  int scale = getDecimalScale(decimal_places);
  int integer_part = (int)rounded_value;
  int decimal_part = (int)round((rounded_value - integer_part) * scale);
  int display_number = integer_part * scale + decimal_part;

  display.showNumberDecEx(display_number, getDecimalDot(decimal_places), false, 4, 0);
}

void showNumber(TM1637Display &display, double value) {
  showNumber(display, value, -1);
}

// ---------------------------------------------------------------------------
// 16x2 LCD with on-demand updates to reduce flicker.
// ---------------------------------------------------------------------------
void showLcdData() {
  if (!lcdNeedsUpdate()) {
    return;
  }

  if (game.status == STATUS_SELECTING_OPERATION) {
    showLcdSelecting();
    saveLcdState();
    return;
  }

  if (game.status == STATUS_SHOWING_CORRECT_ANSWER) {
    showLcdAnswerResult(true);
    saveLcdState();
    return;
  }

  if (game.status == STATUS_SHOWING_INCORRECT_ANSWER) {
    showLcdAnswerResult(false);
    saveLcdState();
    return;
  }

  showLcdGame();
  saveLcdState();
}

// ---------------------------------------------------------------------------
// 8x8 matrix for the current operation.
// ---------------------------------------------------------------------------
void showOperationSymbol(Operation operation) {
  byte *symbol = blank_symbol;

  if (operation == OP_ADD) symbol = add_symbol;
  else if (operation == OP_SUBTRACT) symbol = subtract_symbol;
  else if (operation == OP_MULTIPLY) symbol = multiply_symbol;
  else if (operation == OP_DIVIDE) symbol = divide_symbol;

  for (int i = 0; i < MATRIX_TOTAL_ROWS; i++) {
    led_matrix.setRow(0, i, symbol[i]);
  }
}
