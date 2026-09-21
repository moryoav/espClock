#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include "pizza_renderer.h"
using namespace pizza;
constexpr int kPixels=480*320;
void dump(const std::string &path) {
  std::ofstream f(path,std::ios::binary); f.write(reinterpret_cast<char *>(gFramebuffer),kPixels*2); assert(f.good());
}
int main(int argc,char **argv) {
  assert(argc==2); const std::string out=argv[1]; assert(begin()); auto *allocated=gFramebuffer;
  std::vector<uint16_t> guarded(kPixels+256,0xa55a); gFramebuffer=guarded.data()+128;
  auto bounds=[&](){for(int i=0;i<128;i++){assert(guarded[i]==0xa55a);assert(guarded[kPixels+128+i]==0xa55a);}};
  for (int minute=0;minute<1440;minute++) {
    set_time(minute/60,minute%60,100,true); const int next=(minute+1)%1440;
    int old[4]; for(int i=0;i<4;i++)old[i]=gDigits[i].value;
    set_time(next/60,next%60,200);
    for(int i=0;i<4;i++) {
      auto &d=gDigits[i];assert(d.changing==(old[i]!=d.value));
      if(d.changing)assert(d.started==200);
      d.elapsed(3799);assert(d.changing==(old[i]!=d.value));d.elapsed(3800);assert(!d.changing);
    }
  }
  std::puts("PASS: all 1,440 minute boundaries, changed digits only, synchronized 3600 ms transitions.");
  for(int minute:{0,9,59,599,719,779,1439}) {
    set_time(minute/60,minute%60,100,true);draw_frame(100);esphome::display::Display panel;transfer(panel);
    const int next=(minute+1)%1440;set_time(next/60,next%60,200);
    for(int frame=0;frame<=60;frame++) {
      uint8_t mask=0;for(int i=0;i<4;i++)if(gDigits[i].changing)mask|=1<<i;
      draw_frame(200+frame*60,mask,false);transfer(panel);
      const std::vector<uint16_t> incremental(gFramebuffer,gFramebuffer+kPixels);
      assert(std::equal(incremental.begin(),incremental.end(),panel.panel.begin()));
      draw_frame(200+frame*60);assert(std::equal(incremental.begin(),incremental.end(),gFramebuffer));bounds();
    }
    assert(panel.transferred_pixels<62*kPixels);
  }
  std::puts("PASS: overlapping slices, partial redraws, native rotation and panel transfers match full frames.");
  set_time(12,45,100,true);draw_frame(100);dump(out+"/clock.bin");
  const std::vector<uint16_t> still(gFramebuffer,gFramebuffer+kPixels);
  set_time(12,46,1000);draw_frame(1000);assert(std::equal(still.begin(),still.end(),gFramebuffer));
  for(int frame=0;frame<=60;frame++) {
    draw_frame(1000+frame*60);bounds();
    for(int x=0;x<330;x++)for(int y=0;y<320;y++)assert(gFramebuffer[index(x,y)]==still[index(x,y)]);
    if(frame==32)for(int x=366;x<480;x++)for(int y=0;y<320;y++)assert(gFramebuffer[index(x,y)]==pizza_assets::kBackground[index(x,y)]);
    dump(out+"/frame_"+std::to_string(frame)+".bin");
  }
  const std::vector<uint16_t> settled(gFramebuffer,gFramebuffer+kPixels);
  set_time(12,46,5000,true);draw_frame(5000);assert(std::equal(settled.begin(),settled.end(),gFramebuffer));
  set_time(23,59,5000,true);set_time(0,0,5100);
  for(int frame=0;frame<=60;frame++){draw_frame(5100+frame*60);bounds();dump(out+"/midnight_"+std::to_string(frame)+".bin");}
  set_time(1,1,9000);draw_frame(18000);for(auto &d:gDigits)assert(!d.changing && d.value==d.old_value);
  set_time(3,59,18001);set_time(4,0,18020);draw_frame(22000);
  assert(gDigits[0].value==0 && gDigits[1].value==4 && gDigits[2].value==0 && gDigits[3].value==0);
  set_time(99,-1,22001);assert(gDigits[1].value==4);
  set_time(12,45,0,true);set_time(12,46,UINT32_MAX-100);draw_frame(50);assert(gDigits[3].changing);
  draw_frame(kDurationMs-101);assert(!gDigits[3].changing);
  for(int value=0;value<10;value++) {
    for(auto &d:gDigits)d.set(value,0,true);draw_frame(0);bounds();dump(out+"/glyph_"+std::to_string(value)+".bin");
  }
  set_selected(true);esphome::display::Display display;render(display);assert(display.transfers==1);render(display);assert(display.transfers==1);
  set_selected(false);set_selected(true);assert(!gHaveTime);render(display);assert(display.transfers==2);for(auto &d:gDigits)assert(!d.changing);
  bounds();gFramebuffer=allocated;
  std::puts("PASS: bite removal restores cheese, idle suppression, all glyphs, bounds, midnight, interrupted time, stalled loop, timer rollover and mode re-entry.");
}
