#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
constexpr uint16_t TFT_BLACK=0;
class TFT_eSPI {};
// Host adapter models TFT_eSPI's clipped viewport and origin semantics.
class TFT_eSprite {
  int w=0,h=0,ox=0,oy=0,left=0,top=0,right=0,bottom=0;
 public:
  std::vector<uint16_t> pixels;
  explicit TFT_eSprite(TFT_eSPI*){}
  void createSprite(int width,int height){w=width;h=height;pixels.resize(w*h);resetViewport();}
  void resetViewport(){ox=oy=0;left=top=0;right=w;bottom=h;}
  void setOrigin(int x,int y){ox=x;oy=y;}
  void setViewport(int x,int y,int width,int height,bool datum){
    left=std::max(0,x);top=std::max(0,y);right=std::min(w,x+width);bottom=std::min(h,y+height);
    ox=datum?x:0;oy=datum?y:0;
  }
  void drawPixel(int x,int y,uint16_t c){x+=ox;y+=oy;if(x>=left&&x<right&&y>=top&&y<bottom)pixels[y*w+x]=c;}
  void fillSprite(uint16_t c){for(int y=0;y<h;y++)for(int x=0;x<w;x++)drawPixel(x,y,c);}
};
