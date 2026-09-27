#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <fnk0104b/board.hpp>

namespace {

enum class KeyAction : uint8_t {
  clear,
  erase,
  digit,
  decimal,
  sign,
  operation,
  equals,
};

struct Key {
  const char* label;
  KeyAction action;
  char value;
  uint8_t row;
  uint8_t column;
  uint8_t span;
};

constexpr Key kKeys[] = {
    {"AC", KeyAction::clear, 0, 0, 0, 1},
    {"DEL", KeyAction::erase, 0, 0, 1, 1},
    {"/", KeyAction::operation, '/', 0, 2, 1},
    {"x", KeyAction::operation, '*', 0, 3, 1},
    {"7", KeyAction::digit, '7', 1, 0, 1},
    {"8", KeyAction::digit, '8', 1, 1, 1},
    {"9", KeyAction::digit, '9', 1, 2, 1},
    {"-", KeyAction::operation, '-', 1, 3, 1},
    {"4", KeyAction::digit, '4', 2, 0, 1},
    {"5", KeyAction::digit, '5', 2, 1, 1},
    {"6", KeyAction::digit, '6', 2, 2, 1},
    {"+", KeyAction::operation, '+', 2, 3, 1},
    {"1", KeyAction::digit, '1', 3, 0, 1},
    {"2", KeyAction::digit, '2', 3, 1, 1},
    {"3", KeyAction::digit, '3', 3, 2, 1},
    {"+/-", KeyAction::sign, 0, 3, 3, 1},
    {"0", KeyAction::digit, '0', 4, 0, 2},
    {".", KeyAction::decimal, 0, 4, 2, 1},
    {"=", KeyAction::equals, 0, 4, 3, 1},
};

constexpr int16_t kMarginX = 8;
constexpr int16_t kKeyTop = 58;
constexpr int16_t kCellWidth = 73;
constexpr int16_t kCellHeight = 33;
constexpr int16_t kColumnStep = 77;
constexpr int16_t kRowStep = 36;
constexpr int16_t kKeyGap = 4;
constexpr uint16_t kBackground = 0x10A3;
constexpr uint16_t kDigitButton = 0x2128;
constexpr uint16_t kFunctionButton = 0x39C7;
constexpr uint16_t kOperatorButton = 0x2435;
constexpr uint16_t kEqualsButton = 0xFCA0;

TFT_eSPI& tft = fnk0104b::display.driver();
String input = "0";
String expression;
double accumulator = 0.0;
char pending_operation = 0;
bool fresh_input = false;
bool error_state = false;
bool previous_touch = false;
fnk0104b::TouchPoint point{0, 0, false};

String formatNumber(double value) {
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%.9g", value);
  return String(buffer);
}

double inputValue() { return strtod(input.c_str(), nullptr); }

const char* operationLabel(char operation) {
  switch (operation) {
    case '+':
      return "+";
    case '-':
      return "-";
    case '*':
      return "x";
    case '/':
      return "/";
    default:
      return "";
  }
}

bool calculate(double left, double right, char operation, double& result) {
  switch (operation) {
    case '+':
      result = left + right;
      break;
    case '-':
      result = left - right;
      break;
    case '*':
      result = left * right;
      break;
    case '/':
      if (right == 0.0) {
        return false;
      }
      result = left / right;
      break;
    default:
      return false;
  }
  return isfinite(result);
}

void showError() {
  input = "ERROR";
  expression = "Cannot divide by zero";
  pending_operation = 0;
  fresh_input = true;
  error_state = true;
}

void clearCalculator() {
  input = "0";
  expression = "";
  accumulator = 0.0;
  pending_operation = 0;
  fresh_input = false;
  error_state = false;
}

void appendDigit(char digit) {
  if (error_state || fresh_input) {
    input = "0";
    expression = "";
    error_state = false;
    fresh_input = false;
  }
  if (input.length() >= 12) {
    return;
  }
  if (input == "0") {
    input = String(digit);
  } else if (input == "-0") {
    input = String("-") + digit;
  } else {
    input += digit;
  }
}

void addDecimalPoint() {
  if (error_state || fresh_input) {
    input = "0";
    expression = "";
    error_state = false;
    fresh_input = false;
  }
  if (input.indexOf('.') < 0 && input.length() < 12) {
    input += ".";
  }
}

void eraseLastDigit() {
  if (error_state || fresh_input) {
    input = "0";
    expression = "";
    error_state = false;
    fresh_input = false;
    return;
  }
  if (input.length() > 1) {
    input.remove(input.length() - 1);
    if (input == "-") {
      input = "0";
    }
  } else {
    input = "0";
  }
}

void toggleSign() {
  if (error_state) {
    clearCalculator();
    return;
  }
  if (fresh_input && pending_operation) {
    input = "0";
    expression = "";
    fresh_input = false;
  } else if (fresh_input) {
    fresh_input = false;
  }
  if (input.startsWith("-")) {
    input.remove(0, 1);
  } else {
    input = String("-") + input;
  }
}

void chooseOperation(char operation) {
  if (error_state) {
    return;
  }
  const double right = inputValue();
  if (pending_operation && !fresh_input) {
    double result = 0.0;
    if (!calculate(accumulator, right, pending_operation, result)) {
      showError();
      return;
    }
    accumulator = result;
    input = formatNumber(result);
  } else if (!pending_operation) {
    accumulator = right;
  }
  pending_operation = operation;
  expression = formatNumber(accumulator) + " " + operationLabel(operation);
  input = formatNumber(accumulator);
  fresh_input = true;
}

void equals() {
  if (error_state || !pending_operation) {
    return;
  }
  const double right = fresh_input ? accumulator : inputValue();
  double result = 0.0;
  const String left_text = formatNumber(accumulator);
  const String right_text = formatNumber(right);
  if (!calculate(accumulator, right, pending_operation, result)) {
    showError();
    return;
  }
  expression = left_text + " " + operationLabel(pending_operation) + " " +
               right_text + " =";
  accumulator = result;
  input = formatNumber(result);
  pending_operation = 0;
  fresh_input = true;
}

uint16_t keyColor(const Key& key) {
  if (key.action == KeyAction::clear || key.action == KeyAction::erase ||
      key.action == KeyAction::sign) {
    return kFunctionButton;
  }
  if (key.action == KeyAction::operation) {
    return kOperatorButton;
  }
  if (key.action == KeyAction::equals) {
    return kEqualsButton;
  }
  return kDigitButton;
}

void drawKey(const Key& key) {
  const int16_t x = kMarginX + key.column * kColumnStep;
  const int16_t y = kKeyTop + key.row * kRowStep;
  const int16_t width = key.span * kCellWidth + (key.span - 1) * kKeyGap;
  const uint16_t color = keyColor(key);
  tft.fillRoundRect(x, y, width, kCellHeight, 6, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(key.label, x + width / 2, y + kCellHeight / 2, 2);
}

void drawReadout() {
  constexpr int16_t kExpressionX = 140;
  constexpr int16_t kExpressionY = 4;
  constexpr int16_t kExpressionWidth = 170;
  constexpr int16_t kExpressionCharacters = 14;

  tft.fillRect(kExpressionX, kExpressionY, kExpressionWidth, 17, kBackground);
  tft.fillRect(8, 20, tft.width() - 16, 31, kBackground);

  String visible_expression = expression;
  if (visible_expression.length() > kExpressionCharacters) {
    visible_expression = "..." + visible_expression.substring(
                                      visible_expression.length() -
                                      (kExpressionCharacters - 3));
  }

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, kBackground);
  tft.drawString(visible_expression, tft.width() - 10, 5, 2);
  tft.setTextColor(error_state ? TFT_RED : TFT_WHITE, kBackground);
  tft.drawString(input, tft.width() - 10, 23, 4);
}

void drawCalculator() {
  tft.fillScreen(kBackground);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_CYAN, kBackground);
  tft.drawString("FNK CALCULATOR", 10, 6, 2);

  tft.drawFastHLine(8, 52, tft.width() - 16, 0x4208);
  for (const Key& key : kKeys) {
    drawKey(key);
  }
  drawReadout();
}

void handleKey(const Key& key) {
  switch (key.action) {
    case KeyAction::clear:
      clearCalculator();
      break;
    case KeyAction::erase:
      eraseLastDigit();
      break;
    case KeyAction::digit:
      appendDigit(key.value);
      break;
    case KeyAction::decimal:
      addDecimalPoint();
      break;
    case KeyAction::sign:
      toggleSign();
      break;
    case KeyAction::operation:
      chooseOperation(key.value);
      break;
    case KeyAction::equals:
      equals();
      break;
  }
  Serial.printf("calculator_input=%s\n", input.c_str());
  drawReadout();
}

void handleTouch(int16_t x, int16_t y) {
  for (const Key& key : kKeys) {
    const int16_t key_x = kMarginX + key.column * kColumnStep;
    const int16_t key_y = kKeyTop + key.row * kRowStep;
    const int16_t key_width = key.span * kCellWidth + (key.span - 1) * kKeyGap;
    if (x >= key_x && x < key_x + key_width && y >= key_y &&
        y < key_y + kCellHeight) {
      handleKey(key);
      return;
    }
  }
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("calculator", "1.0.0");
  fnk0104b::display.begin(1);
  const bool touch_ready = fnk0104b::touch.begin();
  Serial.printf("calculator_touch=%s\n", touch_ready ? "ready" : "not_found");
  drawCalculator();
}

void loop() {
  if (fnk0104b::touch.read(point)) {
    if (point.pressed && !previous_touch) {
      handleTouch(point.x, point.y);
    }
    previous_touch = point.pressed;
  } else {
    previous_touch = false;
  }
  delay(12);
}
