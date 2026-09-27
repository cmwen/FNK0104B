#include <Arduino.h>
#include <WiFi.h>
#include <BLEAdvertisedDevice.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <lvgl.h>

#include <array>

#include <fnk0104b/board.hpp>

namespace {

constexpr char kFirmwareVersion[] = "0.2.2";
constexpr char kBleName[] = "FNK0104B-DIAG";
constexpr size_t kMaxWifiResults = 32;
constexpr size_t kMaxBleResults = 5;
constexpr int32_t kExpectedScreenWidth = 320;
constexpr int32_t kExpectedScreenHeight = 240;
constexpr int32_t kKeyboardTop = 113;
constexpr uint32_t kBleScanSeconds = 4;

constexpr uint32_t kColorBackground = 0x101827;
constexpr uint32_t kColorPanel = 0x1C2838;
constexpr uint32_t kColorPanelAlt = 0x243449;
constexpr uint32_t kColorBlue = 0x2478D4;
constexpr uint32_t kColorGreen = 0x168C69;
constexpr uint32_t kColorRed = 0xA93242;
constexpr uint32_t kColorText = 0xF1F5F9;
constexpr uint32_t kColorMuted = 0xA9B7C7;
constexpr uint32_t kColorAccent = 0x58C7D9;

enum class Screen : uint8_t { kWifi, kBle, kJoin };
enum class Action : uintptr_t {
  kWifi = 1,
  kBle,
  kWifiScan,
  kOpenJoin,
  kJoin,
  kBack,
  kBleScan,
  kBleAdvertise,
};

struct WifiResult {
  String ssid;
  int32_t rssi = 0;
  wifi_auth_mode_t auth = WIFI_AUTH_OPEN;
};

struct BleResult {
  String name;
  String address;
  int rssi = 0;
};

Screen current_screen = Screen::kWifi;
String wifi_message = "Checking saved network";
String ble_message = "BLE starting";
std::array<WifiResult, kMaxWifiResults> wifi_results;
std::array<BleResult, kMaxBleResults> ble_results;
size_t wifi_result_count = 0;
size_t ble_result_count = 0;
int16_t wifi_scan_count = 0;
bool wifi_scanning = false;
bool wifi_connecting = false;
bool ble_initialized = false;
bool ble_advertising = false;
bool ble_scanning = false;
bool resume_advertising_after_scan = false;
volatile bool ble_scan_finished = false;
bool previous_touch = false;
uint32_t wifi_connect_started = 0;
fnk0104b::TouchPoint touch_point{0, 0, false};
BLEScan* ble_scanner = nullptr;

lv_display_t* lv_display = nullptr;
lv_indev_t* lv_touch = nullptr;
lv_obj_t* wifi_screen = nullptr;
lv_obj_t* ble_screen = nullptr;
lv_obj_t* join_screen = nullptr;
lv_obj_t* wifi_status_label = nullptr;
lv_obj_t* wifi_detail_label = nullptr;
lv_obj_t* wifi_scan_button = nullptr;
lv_obj_t* wifi_list = nullptr;
lv_obj_t* ble_status_label = nullptr;
lv_obj_t* ble_detail_label = nullptr;
lv_obj_t* ble_scan_button = nullptr;
lv_obj_t* ble_advertise_button = nullptr;
lv_obj_t* ble_count_label = nullptr;
lv_obj_t* ble_list = nullptr;
lv_obj_t* ssid_textarea = nullptr;
lv_obj_t* password_textarea = nullptr;
lv_obj_t* keyboard = nullptr;
lv_obj_t* join_hint_label = nullptr;
int32_t screen_width = 0;
int32_t screen_height = 0;

TFT_eSPI& displayDriver() { return fnk0104b::display.driver(); }

uint32_t lvTick() { return millis(); }

void flushDisplay(lv_display_t* display, const lv_area_t* area,
                  uint8_t* pixel_map) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  TFT_eSPI& tft = displayDriver();
  // LVGL stores RGB565 words in CPU byte order; TFT_eSPI otherwise sends the
  // low byte first on this SPI panel, swapping red/blue components visually.
  tft.setSwapBytes(true);
  tft.startWrite();
  tft.pushImage(area->x1, area->y1, width, height,
                reinterpret_cast<uint16_t*>(pixel_map));
  tft.endWrite();
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t* indev, lv_indev_data_t* data) {
  if (!fnk0104b::touch.read(touch_point)) {
    data->state = previous_touch ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    return;
  }
  previous_touch = touch_point.pressed;
  data->state = touch_point.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  data->point.x = touch_point.x;
  data->point.y = touch_point.y;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                    int32_t width, int32_t height,
                    const lv_font_t* font = &lv_font_montserrat_14,
                    uint32_t color = kColorText) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, width, height);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_scrollable(label, false);
  return label;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                     int32_t width, int32_t height, uint32_t color,
                     Action action, lv_event_cb_t callback);

void setButtonLabel(lv_obj_t* button, const char* text) {
  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label != nullptr) lv_label_set_text(label, text);
}

void styleScreen(lv_obj_t* screen) {
  lv_obj_set_style_bg_color(screen, lv_color_hex(kColorBackground), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
  lv_obj_set_scrollable(screen, false);
}

void styleButton(lv_obj_t* button, uint32_t color) {
  lv_obj_set_style_bg_color(button, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_all(button, 3, LV_PART_MAIN);
  lv_obj_set_style_text_color(button, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_set_style_text_font(button, &lv_font_montserrat_14, LV_PART_MAIN);
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int32_t x, int32_t y,
                     int32_t width, int32_t height, uint32_t color,
                     Action action, lv_event_cb_t callback) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  styleButton(button, color);
  lv_obj_t* label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(action)));
  return button;
}

String abbreviated(const String& value, size_t max_chars) {
  if (value.length() <= max_chars) return value;
  return String("…") + value.substring(value.length() - max_chars + 1);
}

lv_obj_t* createResultList(lv_obj_t* parent, int32_t y) {
  lv_obj_t* list = lv_obj_create(parent);
  lv_obj_set_pos(list, 8, y);
  lv_obj_set_size(list, 304, 98);
  lv_obj_set_style_bg_color(list, lv_color_hex(kColorPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(list, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(list, 5, LV_PART_MAIN);
  lv_obj_set_style_pad_all(list, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_row(list, 2, LV_PART_MAIN);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
  return list;
}

void addResultText(lv_obj_t* list, const char* text) {
  lv_obj_t* label = lv_label_create(list);
  lv_label_set_text(label, text);
  lv_obj_set_size(label, lv_pct(100), 25);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(kColorMuted), LV_PART_MAIN);
  lv_obj_set_style_pad_left(label, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label, 5, LV_PART_MAIN);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

lv_obj_t* addResultButton(lv_obj_t* list, const char* text) {
  lv_obj_t* row = lv_button_create(list);
  lv_obj_set_size(row, lv_pct(100), 25);
  lv_obj_set_style_bg_color(row, lv_color_hex(kColorPanelAlt), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(row, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_hor(row, 8, LV_PART_MAIN);
  lv_obj_set_style_text_color(row, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_set_style_text_font(row, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_t* label = lv_label_create(row);
  lv_label_set_text(label, text);
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
  lv_obj_set_width(label, lv_pct(100));
  lv_obj_center(label);
  return row;
}

const char* wifiStatusLabel() {
  if (wifi_connecting) return "CONNECTING";
  if (WiFi.status() == WL_CONNECTED) return "CONNECTED";
  if (wifi_scanning) return "SCANNING";
  if (WiFi.status() == WL_NO_SSID_AVAIL) return "NETWORK NOT FOUND";
  if (wifi_message.startsWith("Connection failed")) return "CONNECTION FAILED";
  return "NOT CONNECTED";
}

void refreshWifiStatus() {
  if (wifi_status_label == nullptr) return;
  const bool connected = WiFi.status() == WL_CONNECTED;
  lv_label_set_text_fmt(wifi_status_label, "Wi-Fi  %s", wifiStatusLabel());
  lv_obj_set_style_text_color(wifi_status_label,
                              lv_color_hex(connected ? kColorGreen : kColorAccent),
                              LV_PART_MAIN);
  if (connected) {
    lv_label_set_text_fmt(wifi_detail_label, "%s   •   %s",
                          abbreviated(WiFi.SSID(), 18).c_str(),
                          WiFi.localIP().toString().c_str());
  } else {
    lv_label_set_text(wifi_detail_label, abbreviated(wifi_message, 48).c_str());
  }
  if (wifi_scan_button != nullptr) {
    setButtonLabel(wifi_scan_button, wifi_scanning ? "SCANNING..." : "SCAN WI-FI");
  }
}

void refreshWifiList() {
  if (wifi_list == nullptr) return;
  lv_obj_delete(wifi_list);
  wifi_list = createResultList(wifi_screen, 136);

  if (wifi_scanning) {
    addResultText(wifi_list, "Scanning for 2.4 GHz networks...");
  } else if (wifi_scan_count == 0 && wifi_result_count == 0) {
    const char* message = wifi_message.startsWith("No Wi-Fi networks")
                              ? "No networks found. Join manually above."
                              : "Tap SCAN WI-FI to find nearby networks.";
    addResultText(wifi_list, message);
  } else {
    for (size_t i = 0; i < wifi_result_count; ++i) {
      const String network_name = wifi_results[i].ssid.length()
                                      ? abbreviated(wifi_results[i].ssid, 23)
                                      : String("<hidden network>");
      const String row_text = network_name + "    " + String(wifi_results[i].rssi) + " dBm";
      lv_obj_t* row = addResultButton(wifi_list, row_text.c_str());
      lv_obj_add_event_cb(row, [](lv_event_t* event) {
        const uintptr_t index = reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
        if (index > 0 && index - 1 < wifi_result_count) {
          lv_textarea_set_text(ssid_textarea, wifi_results[index - 1].ssid.c_str());
          lv_textarea_set_text(password_textarea, "");
          lv_screen_load(join_screen);
          current_screen = Screen::kJoin;
          lv_keyboard_set_textarea(keyboard, password_textarea);
          lv_obj_send_event(password_textarea, LV_EVENT_FOCUSED, nullptr);
        }
      }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i + 1)));
    }
  }
}

void refreshBleStatus() {
  if (ble_status_label == nullptr) return;
  lv_label_set_text_fmt(ble_status_label, "Bluetooth LE  %s",
                        ble_initialized ? "READY" : "UNAVAILABLE");
  lv_obj_set_style_text_color(ble_status_label,
                              lv_color_hex(ble_initialized ? kColorGreen : kColorRed),
                              LV_PART_MAIN);
  lv_label_set_text(ble_detail_label, abbreviated(ble_message, 48).c_str());
  lv_label_set_text_fmt(ble_count_label, "Nearby devices  %u",
                        static_cast<unsigned>(ble_result_count));
  if (ble_scan_button != nullptr) {
    setButtonLabel(ble_scan_button, ble_scanning ? "SCANNING..." : "SCAN BLE");
    if (ble_scanning) lv_obj_add_state(ble_scan_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(ble_scan_button, LV_STATE_DISABLED);
  }
  if (ble_advertise_button != nullptr) {
    setButtonLabel(ble_advertise_button,
                   ble_advertising ? "STOP ADVERTISING" : "ADVERTISE");
    styleButton(ble_advertise_button, ble_advertising ? kColorRed : kColorGreen);
    if (ble_scanning) lv_obj_add_state(ble_advertise_button, LV_STATE_DISABLED);
    else lv_obj_remove_state(ble_advertise_button, LV_STATE_DISABLED);
  }
}

void refreshBleList() {
  if (ble_list == nullptr) return;
  lv_obj_delete(ble_list);
  ble_list = createResultList(ble_screen, 136);
  if (ble_scanning && ble_result_count == 0) {
    addResultText(ble_list, "Listening for BLE advertisements...");
  } else if (ble_result_count == 0) {
    addResultText(ble_list, "Tap SCAN BLE to find nearby devices.");
  } else {
    for (size_t i = 0; i < ble_result_count; ++i) {
      const String label = ble_results[i].name.length()
                               ? abbreviated(ble_results[i].name, 20)
                               : abbreviated(ble_results[i].address, 20);
      const String row_text = label + "    " + String(ble_results[i].rssi) + " dBm";
      addResultButton(ble_list, row_text.c_str());
    }
  }
}

void onAction(lv_event_t* event);

void createMainScreens() {
  wifi_screen = lv_obj_create(nullptr);
  styleScreen(wifi_screen);
  makeLabel(wifi_screen, "WI-FI CHECK", 8, 7, 190, 23, &lv_font_montserrat_16,
            kColorAccent);
  makeButton(wifi_screen, "BLE", 260, 4, 52, 27, kColorPanelAlt, Action::kBle,
             onAction);
  wifi_status_label = makeLabel(wifi_screen, "Wi-Fi  NOT CONNECTED", 10, 37, 300,
                                22, &lv_font_montserrat_16, kColorAccent);
  wifi_detail_label = makeLabel(wifi_screen, "Checking network...", 10, 61, 300,
                                18, &lv_font_montserrat_14, kColorMuted);
  wifi_scan_button = makeButton(wifi_screen, "SCAN WI-FI", 8, 82, 146, 31,
                                kColorBlue, Action::kWifiScan, onAction);
  makeButton(wifi_screen, "JOIN / EDIT", 166, 82, 146, 31, kColorGreen,
             Action::kOpenJoin, onAction);
  makeLabel(wifi_screen, "NEARBY NETWORKS   •   TAP ONE TO JOIN", 10, 117, 300,
            16, &lv_font_montserrat_12, kColorMuted);
  wifi_list = createResultList(wifi_screen, 136);

  ble_screen = lv_obj_create(nullptr);
  styleScreen(ble_screen);
  makeLabel(ble_screen, "BLUETOOTH LE", 8, 7, 200, 23, &lv_font_montserrat_16,
            kColorAccent);
  makeButton(ble_screen, "WI-FI", 260, 4, 52, 27, kColorPanelAlt,
             Action::kWifi, onAction);
  ble_status_label = makeLabel(ble_screen, "Bluetooth LE  READY", 10, 37, 300,
                               22, &lv_font_montserrat_16, kColorAccent);
  ble_detail_label = makeLabel(ble_screen, "BLE starting", 10, 61, 300, 18,
                               &lv_font_montserrat_14, kColorMuted);
  ble_scan_button = makeButton(ble_screen, "SCAN BLE", 8, 82, 146, 31,
                               kColorBlue, Action::kBleScan, onAction);
  ble_advertise_button = makeButton(ble_screen, "ADVERTISE", 166, 82, 146, 31,
                                    kColorGreen, Action::kBleAdvertise, onAction);
  ble_count_label = makeLabel(ble_screen, "Nearby devices  0", 10, 117, 300,
                              16, &lv_font_montserrat_12, kColorMuted);
  ble_list = createResultList(ble_screen, 136);

  join_screen = lv_obj_create(nullptr);
  styleScreen(join_screen);
  makeButton(join_screen, "BACK", 5, 4, 54, 26, kColorPanelAlt, Action::kBack,
             onAction);
  makeLabel(join_screen, "JOIN WI-FI", 68, 7, 160, 20,
            &lv_font_montserrat_16, kColorAccent);
  makeButton(join_screen, "JOIN", 261, 4, 54, 26, kColorGreen, Action::kJoin,
             onAction);

  ssid_textarea = lv_textarea_create(join_screen);
  lv_obj_set_pos(ssid_textarea, 5, 35);
  lv_obj_set_size(ssid_textarea, 310, 27);
  lv_textarea_set_one_line(ssid_textarea, true);
  lv_textarea_set_max_length(ssid_textarea, 32);
  lv_textarea_set_placeholder_text(ssid_textarea, "Network name (SSID)");
  lv_obj_set_style_bg_color(ssid_textarea, lv_color_hex(kColorPanel), LV_PART_MAIN);
  lv_obj_set_style_text_color(ssid_textarea, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_set_style_text_font(ssid_textarea, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_add_event_cb(ssid_textarea, [](lv_event_t* event) {
    lv_keyboard_set_textarea(keyboard, ssid_textarea);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  }, LV_EVENT_FOCUSED, nullptr);

  password_textarea = lv_textarea_create(join_screen);
  lv_obj_set_pos(password_textarea, 5, 66);
  lv_obj_set_size(password_textarea, 310, 27);
  lv_textarea_set_one_line(password_textarea, true);
  lv_textarea_set_max_length(password_textarea, 63);
  lv_textarea_set_password_mode(password_textarea, true);
  lv_textarea_set_placeholder_text(password_textarea, "Wi-Fi password");
  lv_obj_set_style_bg_color(password_textarea, lv_color_hex(kColorPanel), LV_PART_MAIN);
  lv_obj_set_style_text_color(password_textarea, lv_color_hex(kColorText), LV_PART_MAIN);
  lv_obj_set_style_text_font(password_textarea, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_add_event_cb(password_textarea, [](lv_event_t* event) {
    lv_keyboard_set_textarea(keyboard, password_textarea);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  }, LV_EVENT_FOCUSED, nullptr);
  join_hint_label = makeLabel(join_screen, "Type with the keyboard; tap JOIN when ready.",
                              7, 96, 306, 15, &lv_font_montserrat_12, kColorMuted);

  keyboard = lv_keyboard_create(join_screen);
  const int32_t keyboard_height = screen_height - kKeyboardTop;
  lv_obj_set_size(keyboard, lv_pct(100), keyboard_height);
  // lv_keyboard_create() bottom-aligns itself. Keep that anchor and leave its
  // offset at zero; adding a positive y offset would place most of it offscreen.
  lv_obj_set_align(keyboard, LV_ALIGN_BOTTOM_MID);
  lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_keyboard_set_textarea(keyboard, ssid_textarea);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(kColorPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(keyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_row(keyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_pad_column(keyboard, 2, LV_PART_MAIN);
  lv_obj_set_style_text_font(keyboard, &lv_font_montserrat_14, LV_PART_ITEMS);
  lv_obj_add_event_cb(keyboard, [](lv_event_t* event) {
    if (lv_event_get_code(event) == LV_EVENT_READY) onAction(event);
  }, LV_EVENT_READY, reinterpret_cast<void*>(static_cast<uintptr_t>(Action::kJoin)));
}

void showScreen(Screen screen) {
  current_screen = screen;
  switch (screen) {
    case Screen::kWifi:
      lv_screen_load(wifi_screen);
      refreshWifiStatus();
      break;
    case Screen::kBle:
      lv_screen_load(ble_screen);
      refreshBleStatus();
      break;
    case Screen::kJoin:
      lv_screen_load(join_screen);
      break;
  }
}

void startWifiScan() {
  if (wifi_scanning) return;
  wifi_message = "Scanning nearby 2.4 GHz networks";
  wifi_result_count = 0;
  wifi_scan_count = 0;
  const int16_t result = WiFi.scanNetworks(true, true, false, 160, 0);
  if (result == WIFI_SCAN_FAILED) {
    wifi_message = "Wi-Fi scan could not start";
    Serial.println("wifi_scan=start_failed");
  } else {
    wifi_scanning = true;
    Serial.println("wifi_scan=started");
  }
  refreshWifiStatus();
  refreshWifiList();
}

void finishWifiScan(int16_t count) {
  wifi_scanning = false;
  wifi_scan_count = count;
  wifi_result_count = min(static_cast<size_t>(max<int16_t>(count, 0)),
                          wifi_results.size());
  for (size_t i = 0; i < wifi_result_count; ++i) {
    wifi_results[i].ssid = WiFi.SSID(i);
    wifi_results[i].rssi = WiFi.RSSI(i);
    wifi_results[i].auth = static_cast<wifi_auth_mode_t>(WiFi.encryptionType(i));
  }
  WiFi.scanDelete();
  wifi_message = count > 0 ? String("Found ") + count + " networks"
                           : "No Wi-Fi networks found";
  Serial.printf("wifi_scan=complete count=%d\n", count);
  refreshWifiStatus();
  refreshWifiList();
}

void beginWifiConnect() {
  const String ssid = lv_textarea_get_text(ssid_textarea);
  const String password = lv_textarea_get_text(password_textarea);
  if (!ssid.length()) {
    lv_label_set_text(join_hint_label, "Enter a network name before joining.");
    return;
  }
  WiFi.persistent(true);
  WiFi.disconnect(false, false);
  WiFi.begin(ssid.c_str(), password.c_str());
  wifi_connect_started = millis();
  wifi_connecting = true;
  wifi_message = "Connecting to " + abbreviated(ssid, 30);
  showScreen(Screen::kWifi);
  refreshWifiList();
  Serial.println("wifi_connect=started");
}

void toggleAdvertising() {
  if (!ble_initialized || ble_scanning) return;
  if (ble_advertising) {
    BLEDevice::stopAdvertising();
    ble_advertising = false;
    ble_message = "BLE advertising stopped";
    Serial.println("ble_advertising=off");
  } else {
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    ble_advertising = true;
    ble_message = String("Advertising as ") + kBleName;
    Serial.printf("ble_advertising=on name=%s\n", kBleName);
  }
  refreshBleStatus();
}

void onBleScanComplete(BLEScanResults results) {
  (void)results;
  ble_scan_finished = true;
}

void startBleScan() {
  if (!ble_initialized || ble_scanner == nullptr) {
    ble_message = "BLE is not initialized";
    refreshBleStatus();
    return;
  }
  if (ble_scanning) return;
  resume_advertising_after_scan = ble_advertising;
  if (resume_advertising_after_scan) BLEDevice::stopAdvertising();
  ble_message = "Scanning for BLE advertisements...";
  ble_result_count = 0;
  ble_scan_finished = false;
  ble_scanner->clearResults();
  ble_scanning = ble_scanner->start(kBleScanSeconds, onBleScanComplete, false);
  if (!ble_scanning) {
    ble_message = "BLE scan could not start";
    if (resume_advertising_after_scan) BLEDevice::startAdvertising();
    resume_advertising_after_scan = false;
    Serial.println("ble_scan=start_failed");
  } else {
    Serial.printf("ble_scan=started duration_s=%u\n",
                  static_cast<unsigned>(kBleScanSeconds));
  }
  refreshBleStatus();
  refreshBleList();
}

void finishBleScan() {
  if (!ble_scan_finished) return;
  ble_scan_finished = false;
  ble_scanning = false;
  BLEScanResults found = ble_scanner->getResults();
  const int count = found.getCount();
  ble_result_count = min(static_cast<size_t>(max(count, 0)), ble_results.size());
  for (size_t i = 0; i < ble_result_count; ++i) {
    BLEAdvertisedDevice device = found.getDevice(i);
    ble_results[i].name = device.haveName() ? String(device.getName().c_str()) : "";
    ble_results[i].address = String(device.getAddress().toString().c_str());
    ble_results[i].rssi = device.getRSSI();
  }
  ble_scanner->clearResults();
  if (resume_advertising_after_scan) BLEDevice::startAdvertising();
  ble_advertising = resume_advertising_after_scan;
  resume_advertising_after_scan = false;
  ble_message = String("Scan complete: ") + count + " devices";
  Serial.printf("ble_scan=complete count=%d\n", count);
  refreshBleStatus();
  refreshBleList();
}

void onAction(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED &&
      lv_event_get_code(event) != LV_EVENT_READY) return;
  const Action action = static_cast<Action>(
      reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
  switch (action) {
    case Action::kWifi:
      showScreen(Screen::kWifi);
      break;
    case Action::kBle:
      showScreen(Screen::kBle);
      break;
    case Action::kWifiScan:
      startWifiScan();
      break;
    case Action::kOpenJoin:
      lv_textarea_set_text(ssid_textarea, "");
      lv_textarea_set_text(password_textarea, "");
      lv_keyboard_set_textarea(keyboard, ssid_textarea);
      showScreen(Screen::kJoin);
      break;
    case Action::kJoin:
      beginWifiConnect();
      break;
    case Action::kBack:
      showScreen(Screen::kWifi);
      break;
    case Action::kBleScan:
      startBleScan();
      break;
    case Action::kBleAdvertise:
      toggleAdvertising();
      break;
  }
}

void updateWifiState() {
  if (wifi_scanning) {
    const int16_t result = WiFi.scanComplete();
    if (result != WIFI_SCAN_RUNNING) finishWifiScan(result);
  }

  if (wifi_connecting) {
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connecting = false;
      wifi_message = "Connected";
      Serial.printf("wifi_connect=success ip=%s\n",
                    WiFi.localIP().toString().c_str());
      refreshWifiStatus();
    } else if (millis() - wifi_connect_started > 20000) {
      wifi_connecting = false;
      wifi_message = "Connection failed; check SSID and password";
      WiFi.disconnect(false, false);
      Serial.println("wifi_connect=timeout");
      refreshWifiStatus();
    }
  }
}

void initializeLvgl() {
  screen_width = displayDriver().width();
  screen_height = displayDriver().height();
  Serial.printf("display_resolution=%ldx%ld\n", static_cast<long>(screen_width),
                static_cast<long>(screen_height));
  if (screen_width != kExpectedScreenWidth ||
      screen_height != kExpectedScreenHeight) {
    Serial.printf("display_resolution_warning=expected_%ldx%ld\n",
                  static_cast<long>(kExpectedScreenWidth),
                  static_cast<long>(kExpectedScreenHeight));
  }

  lv_init();
  lv_tick_set_cb(lvTick);
  lv_display = lv_display_create(screen_width, screen_height);
  lv_display_set_color_format(lv_display, LV_COLOR_FORMAT_RGB565);
  static uint16_t draw_buffer[kExpectedScreenWidth * 24];
  lv_display_set_buffers(lv_display, draw_buffer, nullptr, sizeof(draw_buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lv_display, flushDisplay);
  lv_theme_t* theme = lv_theme_default_init(
      lv_display, lv_color_hex(kColorBlue), lv_color_hex(kColorAccent), true,
      LV_FONT_DEFAULT);
  lv_display_set_theme(lv_display, theme);

  lv_touch = lv_indev_create();
  lv_indev_set_type(lv_touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_touch, readTouch);
  lv_timer_set_period(lv_indev_get_read_timer(lv_touch), 45);

  createMainScreens();
  Serial.printf("keyboard_geometry=x0 y=%ld w=%ld h=%ld\n",
                static_cast<long>(kKeyboardTop),
                static_cast<long>(screen_width),
                static_cast<long>(screen_height - kKeyboardTop));
  refreshWifiStatus();
  refreshWifiList();
  refreshBleStatus();
  refreshBleList();
  showScreen(Screen::kWifi);
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("connectivity-diagnostic", kFirmwareVersion);
  fnk0104b::display.begin(1);
  const bool touch_ready = fnk0104b::touch.begin();
  Serial.printf("touch=%s\n", touch_ready ? "ready" : "not_found");
  initializeLvgl();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  const String saved_ssid = WiFi.SSID();
  WiFi.begin();
  if (saved_ssid.length()) {
    wifi_connecting = true;
    wifi_connect_started = millis();
    wifi_message = "Reconnecting to saved network";
  } else {
    wifi_message = "No saved network; tap JOIN / EDIT";
  }
  refreshWifiStatus();
  refreshWifiList();

  BLEDevice::init(kBleName);
  ble_scanner = BLEDevice::getScan();
  ble_scanner->setActiveScan(true);
  ble_scanner->setInterval(100);
  ble_scanner->setWindow(90);
  ble_initialized = true;
  ble_message = String("Ready; phone can scan ") + kBleName;
  Serial.printf("ble_stack=ready name=%s\n", kBleName);
  refreshBleStatus();
}

void loop() {
  updateWifiState();
  finishBleScan();
  lv_timer_handler();
  delay(5);
}
