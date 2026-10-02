#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <initializer_list>
#include <ui/monitor_icons.hpp>

// Board-independent drawing helpers. TFT_eSPI and the host preview share these.
namespace ui { namespace monitor {
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xf8u) << 8) | ((g & 0xfcu) << 3) | (b >> 3);
}
constexpr uint16_t kBackground = rgb(0, 3, 6);
constexpr uint16_t kPanel = rgb(5, 15, 22);
constexpr uint16_t kTrack = rgb(17, 40, 50);
constexpr uint16_t kBorder = rgb(16, 78, 90);
constexpr uint16_t kCyan = rgb(17, 221, 249);
constexpr uint16_t kMint = rgb(42, 246, 162);
constexpr uint16_t kText = rgb(227, 246, 253);
constexpr uint16_t kMuted = rgb(126, 162, 178);
constexpr uint16_t kAmber = rgb(255, 195, 64);
constexpr uint16_t kRed = rgb(255, 99, 121);

inline int remaining(int used) { return used < 0 ? -1 : 100 - used; }

template<class Display>
void frame(Display& d, int x, int y, int w, int h, uint16_t accent) {
  d.fillRect(x+1, y+1, w-2, h-2, kPanel);
  constexpr int c = 5;
  d.drawFastHLine(x+c, y, w-2*c, accent);
  d.drawFastHLine(x+c, y+h-1, w-2*c, accent);
  d.drawFastVLine(x, y+c, h-2*c, accent);
  d.drawFastVLine(x+w-1, y+c, h-2*c, accent);
  d.drawLine(x, y+c, x+c, y, accent);
  d.drawLine(x+w-c-1, y, x+w-1, y+c, accent);
  d.drawLine(x, y+h-c-1, x+c, y+h-1, accent);
  d.drawLine(x+w-c-1, y+h-1, x+w-1, y+h-c-1, accent);
}

template<class Display>
void icon(Display& d, const uint8_t* mask, int w, int h, int x, int y, uint16_t tint) {
  // Reference glow/antialias coverage is blended against the panel color.
  const int tr = ((tint >> 11) & 31)*255/31, tg = ((tint >> 5) & 63)*255/63, tb = (tint & 31)*255/31;
  for (int row=0; row<h; ++row) for (int col=0; col<w; ++col) {
    const int alpha = mask[row*w+col];
    if (!alpha) continue;
    d.drawPixel(x+col, y+row, rgb((tr*alpha+5*(255-alpha))/255,
                                (tg*alpha+15*(255-alpha))/255,
                                (tb*alpha+22*(255-alpha))/255));
  }
}

template<class Display>
void segments(Display& d, int x, int y, int w, int h, int percent, uint16_t accent, int count=10) {
  d.drawRect(x, y, w, h, kBorder);
  for (int i=0; i<count; ++i) {
    const int left=x+2+(w-4)*i/count;
    const int right=x+2+(w-4)*(i+1)/count-1;
    const int width=right-left;
    d.fillRect(left, y+2, width, h-4, kTrack);
    if (percent < 0) continue;
    int amount=percent*count-i*100;
    if (amount<0) amount=0;
    if (amount>100) amount=100;
    const int filled=width*amount/100;
    if (filled>0) d.fillRect(left, y+2, filled, h-4, accent);
  }
}

template<class Display>
void statusBar(Display& d, bool wifi, int rssi, const char* integration,
               bool active, bool attention, bool error, int fiveHour, int weekly) {
  using namespace monitor_icons;
  frame(d, 4, 4, 312, 36, kCyan);
  for (int x : {80, 160, 232}) d.drawFastVLine(x, 11, 22, kBorder);
  icon(d, kWifi, kWifiWidth, kWifiHeight, 9, 13, wifi ? kCyan : kMuted);
  d.setTextColor(kText, kPanel); d.drawString("Wi-Fi", 35, 10, 1);
  d.setTextColor(wifi ? kMint : kRed, kPanel); d.drawString(wifi ? "Online" : "Offline", 35, 25, 1);
  const int strength=!wifi ? 0 : (rssi>=-55 ? 4 : (rssi>=-65 ? 3 : (rssi>=-75 ? 2 : 1)));
  for (int i=0; i<4; ++i) d.fillRect(67+i*3, 19-(2+i*2), 2, 2+i*2, i<strength ? kMint : kTrack);
  const bool connected=!strcmp(integration, "connected");
  const bool degraded=!strcmp(integration, "degraded");
  const uint16_t stateColor=!connected ? (degraded ? kAmber : kRed) :
                            (error ? kRed : (attention ? kAmber : (active ? kCyan : kMint)));
  icon(d, kRobot, kRobotWidth, kRobotHeight, 86, 9, stateColor);
  d.setTextColor(kText, kPanel); d.drawString("Codex", 113, 10, 1);
  d.setTextColor(stateColor, kPanel);
  d.drawString(!connected ? (degraded ? "Check" : "Off") :
               (error ? "Error" : (attention ? "Input" : (active ? "Busy" : "Ready"))), 113, 25, 1);
  char value[16];
  d.setTextColor(kCyan, kPanel); d.drawString("5H", 167, 10, 1);
  if (fiveHour<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",fiveHour);
  d.drawRightString(value,225,10,1); segments(d,166,24,61,11,fiveHour,kCyan,8);
  d.setTextColor(kMint, kPanel); d.drawString("WK", 239, 10, 1);
  if (weekly<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",weekly);
  d.drawRightString(value,309,10,1); segments(d,238,24,71,11,weekly,kMint,8);
}

template<class Display>
void quotaCard(Display& d, int x, const char* title, int percent, bool weekly) {
  using namespace monitor_icons;
  const uint16_t accent=weekly ? kMint : kCyan;
  frame(d,x,60,150,122,accent);
  icon(d,weekly ? kCalendar : kClock,16,16,x+11,70,accent);
  d.setTextColor(kText,kPanel);d.drawString(title,x+34,70,2);
  d.drawFastHLine(x+10,92,130,kBorder);
  char value[16]; if(percent<0) snprintf(value,sizeof(value),"--%%"); else snprintf(value,sizeof(value),"%d%%",percent);
  d.setTextColor(accent,kPanel);d.drawCentreString(value,x+75,100,4);
  d.setTextColor(kMuted,kPanel);d.drawCentreString("remaining",x+75,133,1);
  segments(d,x+11,153,128,14,percent,accent);
}

template<class Display>
void voiceControl(Display& d, bool recording, bool preparing, bool busy, bool selected) {
  using namespace monitor_icons;
  const uint16_t accent=recording ? kRed : (busy ? kCyan : kMint);
  frame(d,4,191,312,46, kBorder);
  d.drawCircle(31,214,19,kBorder);d.drawCircle(31,214,17,accent);
  icon(d,kMicrophone,kMicrophoneWidth,kMicrophoneHeight,22,200,accent);
  const char* label=recording ? "Stop recording" : (preparing ? "Preparing mic" :
                    (busy ? "Sending voice" : (selected ? "Reply to agent" : "Voice command")));
  d.setTextColor(kText,kPanel);d.drawString(label,61,199,2);
  d.setTextColor(accent,kPanel);d.drawString(recording ? "Tap to stop" : (preparing ? "Please wait" : (busy ? "Transcribing..." : "Tap to talk")),62,222,1);
  const int lengths[7]={4,9,16,25,16,9,4};
  for(int i=0;i<7;++i) d.drawFastVLine(278+i*4,214-lengths[i]/2,lengths[i],accent);
}
} }  // namespace ui::monitor
