#include <Arduino.h>
#include "LedControl.h"
#include "binary.h"
#include <TM1637Display.h>
#include <LiquidCrystal_I2C.h>

const uint8_t LCD_ADDRESS = 0x27;
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
const int PRESSED_BUTTON_LEVEL = HIGH;

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
  int operand_1 = 0;
  int operand_2 = 0;
  double operation_result = 0;
  Operation current_operation = OP_NONE;
  int level = 0;
  int correct_answers = 0;
  int incorrect_answers = 0;
  GameStatus status = STATUS_WAITING_FOR_ANSWER;
  bool previous_button_readings[TOTAL_BUTTONS] = {LOW, LOW, LOW, LOW};
  bool stable_button_states[TOTAL_BUTTONS] = {LOW, LOW, LOW, LOW};
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
};

struct LCDState {
  bool initialised = false;
  GameStatus status = STATUS_WAITING_FOR_ANSWER;
  int level = LEVEL_EASY;
  int correct_answers = 0;
  int incorrect_answers = 0;
  Operation operation = OP_NONE;
};

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);
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
    // The current hardware uses external resistors, with a pressed button read as HIGH.
    pinMode(answer_button_pins[i], INPUT);
  }

  pinMode(reset_button_pin, INPUT);
  pinMode(potentiometer_pin, INPUT);

  lcd.init();
  lcd.setBacklight(HIGH);
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
  game.status = STATUS_SELECTING_OPERATION;
  game.round_started = false;
}

void resetAllDisplays() {
  for (int i = 0; i < TOTAL_TM1637_DISPLAYS; i++) {
    resetDisplay(*tm1637_displays[i]);
  }
}

void clearLcdLine(int row) {
  lcd.setCursor(0, row);
  lcd.print(F("                "));
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
    return clampInt(game.operand_2 / 2, 1, MAX_WRONG_ANSWER_OFFSET);
  }

  if (game.current_operation == OP_MULTIPLY) {
    return clampInt(max(game.operand_1, game.operand_2) / 2, 2, 15);
  }

  return clampInt(game.operand_2, 1, MAX_WRONG_ANSWER_OFFSET);
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
  if (game.level == LEVEL_EASY) return true;
  return random(0, 2) == 0;
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

  int divisor = random(2, getDivisorLimitForLevel() + 1);
  if (shouldGenerateExactDivision()) {
    int maximum_quotient = max(1, getDividendLimitForLevel() / divisor);
    second_draw = divisor;
    first_draw = divisor * random(1, maximum_quotient + 1);
  } else {
    first_draw = random(1, getDividendLimitForLevel() + 1);
    second_draw = divisor;
  }
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
    if (reading >= EASY_POTENTIOMETER_LIMIT + LEVEL_HYSTERESIS) return LEVEL_MEDIUM;
    return LEVEL_EASY;
  }

  if (game.level == LEVEL_MEDIUM) {
    if (reading <= EASY_POTENTIOMETER_LIMIT - LEVEL_HYSTERESIS) return LEVEL_EASY;
    if (reading >= MEDIUM_POTENTIOMETER_LIMIT + LEVEL_HYSTERESIS) return LEVEL_HARD;
    return LEVEL_MEDIUM;
  }

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
  lcd.setCursor(0, 0);
  lcd.print(F("Mode: "));
  lcd.print(getLevelName());
  lcd.setCursor(0, 1);
  lcd.print(F("Selecting..."));
}

void showLcdAnswerResult(bool is_correct) {
  clearLcdLine(0);
  clearLcdLine(1);
  lcd.setCursor(0, 0);
  lcd.print(is_correct ? F("Right Answer") : F("Wrong Answer"));
  lcd.setCursor(0, 1);
  lcd.print(F("Right:"));
  lcd.print(game.correct_answers);
  lcd.print(F(" Wrong:"));
  lcd.print(game.incorrect_answers);
}

void showLcdGame() {
  clearLcdLine(0);
  clearLcdLine(1);
  lcd.setCursor(0, 0);
  lcd.print(getLevelName());
  lcd.print(" ");
  lcd.print(getShortOperationName());
  lcd.setCursor(0, 1);
  lcd.print(F("Right:"));
  lcd.print(game.correct_answers);
  lcd.print(F(" Wrong:"));
  lcd.print(game.incorrect_answers);
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
        showNumber(*game.correct_answer_display, game.operation_result);
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
double performOperation(int number_1, int number_2, Operation operation) {
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

  do {
    drawOperands(first_draw, second_draw);
    game.operand_1 = first_draw;
    game.operand_2 = second_draw;
    game.operation_result = performOperation(first_draw, second_draw, game.current_operation);
  } while (game.operation_result < 0 || game.operation_result >= 10000);

  showNumber(number_display_1, first_draw);
  showNumber(number_display_2, second_draw);
  Serial.println(String(F("operation result: ")) + String(game.operation_result));
}

void shuffleDisplayPositions() {
  game.correct_answer_position = random(1, 4);
  double answers[3] = {game.operation_result, 0, 0};
  int decimal_places = getDecimalPlaces(game.operation_result);

  if (decimal_places == 0) {
    do {
      int offset_1 = (int)getWrongAnswerStep(0);
      int offset_2 = (int)getWrongAnswerStep(0);
      answers[1] = game.operation_result + offset_1;
      answers[2] = game.operation_result - offset_2;
      if (answers[2] < 0) {
        answers[2] = game.operation_result + offset_1 + offset_2;
      }
    } while (answers[1] == game.operation_result ||
             answers[2] == game.operation_result ||
             answers[1] == answers[2]);
  } else {
    do {
      answers[1] = roundToDecimalPlaces(game.operation_result + getWrongAnswerStep(decimal_places), decimal_places);
      answers[2] = roundToDecimalPlaces(game.operation_result - getWrongAnswerStep(decimal_places), decimal_places);
      if (answers[2] < 0) {
        answers[2] = roundToDecimalPlaces(game.operation_result + getWrongAnswerStep(decimal_places), decimal_places);
      }
    } while (answers[1] == game.operation_result ||
             answers[2] == game.operation_result ||
             answers[1] == answers[2]);
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

  for (int i = 0; i < TOTAL_ANSWER_DISPLAYS; i++) {
    showNumber(*tm1637_displays[i + 2], answers[i]);
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

void showNumber(TM1637Display &display, double value) {
  if (value < 0 || value >= 10000) {
    resetDisplay(display);
    return;
  }

  int decimal_places = getDecimalPlaces(value);
  double rounded_value = roundToDecimalPlaces(value, decimal_places);
  decimal_places = getDecimalPlaces(rounded_value);

  if (decimal_places == 0) {
    display.showNumberDec((int)round(value), false);
    return;
  }

  int scale = getDecimalScale(decimal_places);
  int integer_part = (int)rounded_value;
  int decimal_part = (int)round((rounded_value - integer_part) * scale);
  int display_number = integer_part * scale + decimal_part;

  display.showNumberDecEx(display_number, getDecimalDot(decimal_places), false, 4, 0);
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
