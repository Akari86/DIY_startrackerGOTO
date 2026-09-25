#include <cassert>
#include <iostream>
#include "../BandCanvas.h"
template<class Canvas>void scene(Canvas &c){
  for(int y=0;y<240;y++)for(int x=0;x<320;x++)c.drawPixel(x,y,((x+y)&1)?0x05FA:0xFB20);
  c.setViewport(28,106,105,83,false);
  for(int y=90;y<200;y++)for(int x=20;x<145;x++)c.drawPixel(x,y,0xFE00);
  c.resetViewport();c.drawPixel(10,239,0xFFFF);
  c.setViewport(51,60,265,179,false);
  for(int y=59;y<240;y++)c.drawPixel(317,y,0x1234); // Outside catalogue clip.
  c.resetViewport();
  c.setViewport(100,75,10,10,true);c.drawPixel(1,1,0x7777);
  c.resetViewport();
}
int main(){
  TFT_eSPI tft;TFT_eSprite full(&tft);full.createSprite(320,240);scene(full);
  for(int h:{40,20,10}){
    BandCanvas strip(&tft);strip.createSprite(320,h);std::vector<uint16_t> out(320*240);
    for(int y=0;y<240;y+=h){
      strip.beginBand(y);scene(strip);
      std::copy(strip.pixels.begin(),strip.pixels.end(),out.begin()+320*y);
    }
    assert(out==full.pixels);
    strip.beginBand(200);for(auto p:strip.pixels)assert(p==TFT_BLACK);
  }
  std::cout<<"PASS: 40/20/10-row strips match full-frame coordinates, RGB565 colors, viewport clipping, origin reset and clearing\n";
}
