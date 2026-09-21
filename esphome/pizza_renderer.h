#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <esp_heap_caps.h>
#include "esphome/components/display/display.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "pizza_assets.h"

namespace pizza {
constexpr int kWidth=480, kHeight=320, kTop=107;
constexpr int kPositions[]={53,141,281,366};
constexpr uint32_t kDurationMs=3600;
struct Digit {
  int old_value=-1, value=-1;
  uint32_t started=0;
  bool changing=false;
  void set(int next, uint32_t now, bool snap) {
    if (snap || value<0) { old_value=value=next; changing=false; }
    else if (next!=value) { old_value=value; value=next; started=now; changing=true; }
  }
  uint32_t elapsed(uint32_t now) {
    const uint32_t ms=now-started;
    if (changing && ms>=kDurationMs) { old_value=value; changing=false; }
    return changing ? ms : kDurationMs;
  }
};
inline Digit gDigits[4];
inline uint16_t *gFramebuffer=nullptr;
inline bool gSelected=false, gDirty=true, gFullRedraw=true, gHaveTime=false, gAllocationFailed=false;
inline int gLeft=0, gRight=kWidth;
inline uint32_t gFrames=0, gStarted=0, gMaxWork=0;
inline size_t index(int x,int y) { return size_t(x)*320+319-y; }
inline uint16_t swap16(uint16_t c) { return uint16_t((c<<8)|(c>>8)); }
inline bool selected() { return gSelected; }
inline void set_selected(bool enabled) {
  if (gSelected!=enabled) {
    gDirty=gFullRedraw=true; gHaveTime=false; gFrames=0;
    for (auto &d:gDigits) d=Digit{};
  }
  gSelected=enabled;
}
inline bool begin() {
  if (gFramebuffer) return true;
  if (gAllocationFailed) return false;
  gFramebuffer=static_cast<uint16_t *>(heap_caps_malloc(kWidth*kHeight*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!gFramebuffer) { gAllocationFailed=true; ESP_LOGE("pizza","Unable to allocate PSRAM framebuffer"); return false; }
  ESP_LOGI("pizza","Ready: approved pepperoni artwork, 3600 ms bite and rebuild, 480x320");
  return true;
}
inline void set_time(int hour,int minute,uint32_t now,bool snap=false) {
  if (hour<0 || hour>23 || minute<0 || minute>59) return;
  const int values[]={hour/10,hour%10,minute/10,minute%10};
  for (int i=0;i<4;i++) { if (gDigits[i].value!=values[i] || snap) gDirty=true; gDigits[i].set(values[i],now,snap); }
}
inline void composite(int x,int y,uint32_t src) {
  if (!src) return;
  const unsigned inv=255-(src>>24);
  auto &pixel=gFramebuffer[index(x,y)]; const unsigned dst=swap16(pixel);
  const unsigned r=std::min<uint32_t>(255u,((src>>16)&255)+((dst>>11)*255/31*inv+127)/255);
  const unsigned g=std::min<uint32_t>(255u,((src>>8)&255)+(((dst>>5)&63)*255/63*inv+127)/255);
  const unsigned b=std::min<uint32_t>(255u,(src&255)+((dst&31)*255/31*inv+127)/255);
  pixel=swap16(uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3)));
}
inline uint32_t sample(const uint32_t *sprite,float x,float y,unsigned opacity) {
  const int ix=int(std::floor(x)),iy=int(std::floor(y));
  if (ix<0 || iy<0 || ix>=71 || iy>=71) return 0;
  const unsigned fx=unsigned((x-ix)*256),fy=unsigned((y-iy)*256);
  const unsigned weights[]={(256-fx)*(256-fy),fx*(256-fy),(256-fx)*fy,fx*fy};
  const uint32_t colors[]={sprite[ix*72+iy],sprite[(ix+1)*72+iy],sprite[ix*72+iy+1],sprite[(ix+1)*72+iy+1]};
  uint32_t out=0;
  for (int shift:{0,8,16,24}) {
    unsigned v=0; for (int i=0;i<4;i++) v+=((colors[i]>>shift)&255)*weights[i];
    out|=(((v+32768)>>16)*opacity+127)/255<<shift;
  }
  return out;
}
inline void falling_piece(float x,float y,int variant,float placement,float angle) {
  if (placement<=0) return;
  const float p=std::min(1.0f,placement),lift=(1-p)*(1-p)*(1-p);
  x+=lift*((variant%3)-1)*7; y-=lift*23; angle+=lift*.16f;
  const float scale=31.0f*(1+lift*.45f)/64,cs=std::cos(angle),sn=std::sin(angle);
  const float radius=36*scale*(std::abs(cs)+std::abs(sn));
  const int left=std::max(gLeft,int(std::floor(x-radius))),right=std::min(gRight,int(std::ceil(x+radius)));
  const int top=std::max(0,int(std::floor(y-radius))),bottom=std::min(kHeight,int(std::ceil(y+radius)));
  const auto *sprite=pizza_assets::kSprites+variant*72*72;
  const unsigned opacity=unsigned(std::min(1.0f,p*4)*255+.5f);
  // Follow native panel columns for coherent PSRAM writes.
  for (int dx=left;dx<right;dx++) for (int dy=top;dy<bottom;dy++) {
    const float px=dx+.5f-x,py=dy+.5f-y;
    composite(dx,dy,sample(sprite,(cs*px+sn*py)/scale+35.5f,(-sn*px+cs*py)/scale+35.5f,opacity));
  }
}
inline void cached_piece(int x,int y,int id) {
  const auto &sprite=pizza_assets::kCached[id];
  x+=sprite.x; y+=sprite.y;
  for (unsigned i=0;i<sprite.count;i++) {
    const auto &span=pizza_assets::kSpans[sprite.first+i];
    const int dx=x+span.x;
    if (dx<gLeft || dx>=gRight) continue;
    for (unsigned j=0;j<span.length;j++) {
      const int dy=y+span.y+j;
      if (dy>=0 && dy<kHeight) composite(dx,dy,pizza_assets::kCachedPixels[span.offset+j]);
    }
  }
}
inline void draw_digit(int left,Digit &digit,uint32_t now) {
  const auto ms=digit.elapsed(now);
  const bool eating=digit.changing && ms<2000,placing=digit.changing && !eating;
  const int value=eating?digit.old_value:digit.value;
  if (value<0 || value>9) return;
  for (int i=0;i<pizza_assets::kCounts[value];i++) {
    const auto &p=pizza_assets::kPoints[value][i];
    const int phase=eating?std::clamp(1+int(std::floor((int(ms)-110-i*29)/370.0f)),0,4):0;
    const float placement=placing?(int(ms)-2050-i*65)/440.0f:1.0f;
    if (phase>=4 || placement<=0) continue;
    if (placement>=1) cached_piece(left,kTop,pizza_assets::kDigitOffsets[value]+i*4+phase);
    else falling_piece(left+p.x,kTop+p.y,(value*7+i*5)%6,placement,((i*17+value*7)%29-14)*3.14159265359f/180);
  }
}
inline void draw_frame(uint32_t now,uint8_t mask=15,bool full=true) {
  gLeft=full?0:kWidth; gRight=full?kWidth:0;
  if (!full) for (int i=0;i<4;i++) if (mask&(1<<i)) {
    gLeft=std::min(gLeft,std::max(0,kPositions[i]-36));
    gRight=std::max(gRight,std::min(kWidth,kPositions[i]+104));
  }
  if (gRight<=gLeft) return;
  std::copy(pizza_assets::kBackground+gLeft*320,pizza_assets::kBackground+gRight*320,gFramebuffer+gLeft*320);
  // Neighboring slices can extend into the changed strip while dropping.
  // Repaint every overlapping object, clipped to the restored strip.
  for (int i=0;i<4;i++) draw_digit(kPositions[i],gDigits[i],now);
  cached_piece(239,137,pizza_assets::kColon); cached_piece(239,183,pizza_assets::kColon+1);
}
inline void transfer(esphome::display::Display &display) {
  if (gRight<=gLeft) return;
  display.draw_pixels_at(0,gLeft,320,gRight-gLeft,reinterpret_cast<const uint8_t *>(gFramebuffer+gLeft*320),
                        esphome::display::COLOR_ORDER_RGB,esphome::display::COLOR_BITNESS_565,true);
}
inline void render(esphome::display::Display &display) {
  if (!begin()) { display.fill(esphome::Color(245,180,80)); return; }
  const uint32_t now=esphome::millis(); const time_t epoch=::time(nullptr);
  if (epoch>1700000000) { std::tm local{}; localtime_r(&epoch,&local); set_time(local.tm_hour,local.tm_min,now,!gHaveTime); gHaveTime=true; }
  uint8_t mask=0; for (int i=0;i<4;i++) if (gDigits[i].changing) mask|=1<<i;
  if (!gDirty && !mask) return;
  const auto start=esphome::micros();
  if (mask && gFrames++==0) { gStarted=now; gMaxWork=0; }
  draw_frame(now,mask,gFullRedraw || !mask); transfer(display); gDirty=gFullRedraw=false;
  if (mask) {
    gMaxWork=std::max(gMaxWork,esphome::micros()-start);
    bool finished=true; for (const auto &d:gDigits) finished&=!d.changing;
    if (finished) { ESP_LOGI("pizza","Bites complete: %u ms, %u frames, max draw/send %.1f ms",unsigned(now-gStarted),unsigned(gFrames),gMaxWork/1000.0f); gFrames=0; }
  } else ESP_LOGI("pizza","Frame ready, draw/send %.1f ms",(esphome::micros()-start)/1000.0f);
}
}  // namespace pizza
