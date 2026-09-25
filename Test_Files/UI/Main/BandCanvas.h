#pragma once
#include <TFT_eSPI.h>

// Render with full-screen coordinates into a small RGB565 strip.
// Explicit base calls are essential: clearing uses local coordinates, while
// UI viewports must retain the -top translation after being reset.
class BandCanvas : public TFT_eSprite {
  int top=0;
 public:
  explicit BandCanvas(TFT_eSPI *display):TFT_eSprite(display){}
  void beginBand(int y) {
    top=y;
    TFT_eSprite::resetViewport();
    TFT_eSprite::fillSprite(TFT_BLACK);
    TFT_eSprite::setOrigin(0,-top);
  }
  void resetViewport() {
    TFT_eSprite::resetViewport();
    TFT_eSprite::setOrigin(0,-top);
  }
  void setViewport(int32_t x,int32_t y,int32_t w,int32_t h,bool datum=true) {
    TFT_eSprite::setViewport(x,y-top,w,h,false);
    TFT_eSprite::setOrigin(datum?x:0,datum?y-top:-top);
  }
};
