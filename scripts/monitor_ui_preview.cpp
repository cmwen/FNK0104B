// Host preview of the shared monitor theme using the pinned TFT_eSPI font data.
#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <string>
#include <vector>
#define PROGMEM
#include <Fonts/glcdfont.c>
#include <Fonts/Font16.h>
#include <Fonts/Font32rle.h>
#include <ui/monitor_theme.hpp>
#include <ui/avatar_assets.hpp>
#include <ui/micro_layout.hpp>
#include "../lib/speech/src/monitor_commands.hpp"

class Display {
  std::vector<uint16_t> pixels = std::vector<uint16_t>(320*240);
  uint16_t foreground=0xffff, background=0;
public:
  void drawPixel(int x,int y,uint16_t c){if(x>=0&&x<320&&y>=0&&y<240)pixels[y*320+x]=c;}
  void fillRect(int x,int y,int w,int h,uint16_t c){for(int j=0;j<h;++j)for(int i=0;i<w;++i)drawPixel(x+i,y+j,c);}
  void fillScreen(uint16_t c){fillRect(0,0,320,240,c);}
  void drawFastHLine(int x,int y,int w,uint16_t c){fillRect(x,y,w,1,c);}
  void drawFastVLine(int x,int y,int h,uint16_t c){fillRect(x,y,1,h,c);}
  void drawRect(int x,int y,int w,int h,uint16_t c){drawFastHLine(x,y,w,c);drawFastHLine(x,y+h-1,w,c);drawFastVLine(x,y,h,c);drawFastVLine(x+w-1,y,h,c);}
  void drawLine(int x,int y,int xx,int yy,uint16_t c){int dx=std::abs(xx-x),sx=x<xx?1:-1,dy=-std::abs(yy-y),sy=y<yy?1:-1,e=dx+dy;for(;;){drawPixel(x,y,c);if(x==xx&&y==yy)break;int e2=2*e;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
  void drawCircle(int x,int y,int r,uint16_t c){int xx=r,yy=0,e=1-r;while(xx>=yy){for(int s:{-1,1})for(int t:{-1,1}){drawPixel(x+s*xx,y+t*yy,c);drawPixel(x+s*yy,y+t*xx,c);}++yy;if(e<0)e+=2*yy+1;else{--xx;e+=2*(yy-xx)+1;}}}
  void fillRoundRect(int x,int y,int w,int h,int r,uint16_t c){for(int j=0;j<h;++j)for(int i=0;i<w;++i){int dx=i<r?r-i:(i>=w-r?i-(w-r-1):0),dy=j<r?r-j:(j>=h-r?j-(h-r-1):0);if(dx*dx+dy*dy<=r*r)drawPixel(x+i,y+j,c);}}
  void setTextColor(uint16_t f,uint16_t b){foreground=f;background=b;}
  int textWidth(const char* text,int f){int w=0;for(auto c:std::string(text)){unsigned idx=static_cast<unsigned char>(c);w+=f==1?6:(idx>=32&&idx<128?(f==2?widtbl_f16[idx-32]:widtbl_f32[idx-32]):0);}return w;}
  void drawString(const char* text,int x,int y,int f){for(auto c:std::string(text)){unsigned idx=static_cast<unsigned char>(c);if(idx<32||idx>=128)continue;int w=f==1?6:(f==2?widtbl_f16[idx-32]:widtbl_f32[idx-32]);int h=f==1?8:(f==2?16:26);fillRect(x,y,w,h,background);
    if(f==1){for(int col=0;col<5;++col)for(int row=0;row<8;++row)if(font[idx*5+col]&(1<<row))drawPixel(x+col,y+row,foreground);}
    else if(f==2){const auto data=chrtbl_f16[idx-32];int stride=(w+6)/8;for(int row=0;row<h;++row)for(int col=0;col<w-1;++col)if(data[row*stride+col/8]&(0x80>>(col%8)))drawPixel(x+col,y+row,foreground);}
    else{const auto data=chrtbl_f32[idx-32];int pc=0,pos=0;while(pc<w*h){uint8_t b=data[pos++];int n=(b&127)+1;for(int i=0;i<n&&pc<w*h;++i,++pc)if(b&128)drawPixel(x+pc%w,y+pc/w,foreground);}}
    x+=w;
  }}
  void drawRightString(const char* text,int x,int y,int f){drawString(text,x-textWidth(text,f),y,f);}
  void drawCentreString(const char* text,int x,int y,int f){drawString(text,x-textWidth(text,f)/2,y,f);}
  void avatar(int x,int y,int size,ui::avatar::Mood mood){ui::avatar::Canvas c;ui::avatar::render(c,42,mood,12);for(int j=0;j<size;++j)for(int i=0;i<size;++i)drawPixel(x+i,y+j,c.pixels[(j*32/size)*32+i*32/size]);}
  void save(const std::string& path){std::ofstream f(path,std::ios::binary);f<<"P6\n320 240\n255\n";for(auto c:pixels){char rgb[3]={char(((c>>11)&31)*255/31),char(((c>>5)&63)*255/63),char((c&31)*255/31)};f.write(rgb,3);}}
};
int main(int argc,char**argv){
  const std::string base=argc>1?argv[1]:"/tmp/monitor";
  for(const std::string state:{"idle","active","attention","offline","recording","commands","commands-quiet","status-active","empty","full","micro","micro-six","micro-keys","micro-ble"}){
    const bool micro = state.rfind("micro", 0) == 0;
    Display d;bool active=state=="active"||state=="attention";bool offline=state=="offline";bool recording=state=="recording";bool commands=state=="commands"||state=="commands-quiet";
    d.fillScreen(ui::monitor::kBackground);
    const int fiveHour=offline?-1:(state=="empty"?0:(state=="full"?100:74));
    const int weekly=offline?-1:(state=="empty"?0:(state=="full"?100:12));
    ui::monitor::statusBar(d,!offline,-58,offline?"unavailable":"connected",active||state=="status-active"||commands,state=="attention",false,fiveHour,weekly,
        offline?-1:(state=="empty"?0:(state=="full"?16:8)),
        offline?-1:(state=="empty"?0:(state=="full"?24:12)), micro ? "Linked" : nullptr);
    d.setTextColor(ui::monitor::kMuted,ui::monitor::kBackground);
    d.drawString(micro ? (state=="micro-ble" ? "BLE MICRO / SPEECH TO ORCHESTRATOR" : state=="micro-keys" ? "DESKTOP KEYS / TAP MICRO FOR AGENTS" : "USB MICRO / TAP MICRO FOR KEYS") : offline?"Status HTTP -11":(commands?"Listening for device command":(recording?"Recording Codex message":(active?"ACTIVE AGENTS / QUOTA LEFT":"QUOTA LEFT"))),10,45,1);
    if(micro) for(unsigned i=0;i<(state=="micro"?3u:6u);++i) ui::micro::tile(d,i,i==2?ui::monitor::kAmber:ui::monitor::kCyan,true,i==1,state=="micro-keys");
    else if(commands) ui::monitor::commandHelp(d,speech::kMonitorCommands,speech::kMonitorCommandCount);
    else if(recording) ui::monitor::messagePanel(d,true,false,false);
    else if(!active){ui::monitor::quotaCard(d,6,"5H left",fiveHour,false);ui::monitor::quotaCard(d,164,"Week left",weekly,true);}
    else{ui::monitor::frame(d,8,59,304,123,ui::monitor::kBorder);d.fillRoundRect(15,71,102,102,6,state=="attention"?ui::monitor::kAmber:ui::monitor::kMint);d.avatar(18,74,96,state=="attention"?ui::avatar::Mood::NeedsInput:ui::avatar::Mood::Thinking);d.setTextColor(ui::monitor::kText,ui::monitor::kPanel);d.drawString("Fix display",128,70,2);d.setTextColor(state=="attention"?ui::monitor::kAmber:ui::monitor::kMint,ui::monitor::kPanel);d.drawString(state=="attention"?"Needs input":"Working",128,96,1);d.setTextColor(ui::monitor::kText,ui::monitor::kPanel);d.drawString(state=="attention"?"Which layout should I use?":"Codex is working",128,122,1);}
    if (state=="micro") ui::micro::directions(d);
    if (micro) ui::micro::voice(d,state!="micro-ble",true,false,true,75);
    else ui::monitor::voiceControl(d,recording,false,recording,false,commands,state=="commands-quiet"?0:75,true,true);
    d.save(base+"-"+state+".ppm");
  }
}
