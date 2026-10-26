#pragma once
#include <array>
#include <algorithm>
#include <cmath>
struct SampleRect {float x0,y0,x1,y1;};
struct LedPoint {float x,y;};
// Canonical coordinates always face the FRONT of the display.
// Physical wire: bottom-right start -> BR -> TR -> TL -> BL -> bottom-left end.
inline bool validCorners(const std::array<int,4>& c){
 if(c[0]<1||c[3]>45||c[0]>=c[1]||c[1]>=c[2]||c[2]>=c[3])return false;
 int width=c[2]-c[1];return (c[0]-1)+(45-c[3])<width;
}
inline std::array<LedPoint,45> cornerPoints(const std::array<int,4>&c){
 std::array<LedPoint,45> p{};if(!validCorners(c))return p;
 float width=float(c[2]-c[1]);
 for(int i=0;i<45;i++){int n=i+1;
  if(n<=c[0])p[i]={1-(c[0]-n)/width,1};
  else if(n<=c[1])p[i]={1,1-float(n-c[0])/(c[1]-c[0])};
  else if(n<=c[2])p[i]={1-float(n-c[1])/width,0};
  else if(n<=c[3])p[i]={0,float(n-c[2])/(c[3]-c[2])};
  else p[i]={float(n-c[3])/width,1};
 }return p;
}
inline std::array<SampleRect,45> cornerRegions(const std::array<int,4>&c,float depth,float aspect){
 std::array<SampleRect,45> result{};if(!validCorners(c)||!std::isfinite(aspect)||!std::isfinite(depth)||aspect<=0||depth<=0)return result;auto p=cornerPoints(c);
 // Ordinary LEDs cover one LED spacing along the edge. Depth only changes
 // the inward extent; corner LEDs retain their original corner patches.
 float dx=std::min(.45f,depth/aspect),dy=std::min(.45f,depth);
 for(int i=0;i<45;i++){int n=i+1;auto v=p[i];SampleRect r{};bool corner=std::find(c.begin(),c.end(),n)!=c.end();
  if(corner)r={v.x==0?0:1-dx,v.y==0?0:1-dy,v.x==0?dx:1,v.y==0?dy:1};
  else if(n<c[0]||n>c[3]||(n>c[1]&&n<c[2])){float half=.5f/(c[2]-c[1]);r={std::max(0.f,v.x-half),v.y==0?0:1-dy,std::min(1.f,v.x+half),v.y==0?dy:1};}
  else {float half=.5f/(n<c[1]?c[1]-c[0]:c[3]-c[2]);r={v.x==0?0:1-dx,std::max(0.f,v.y-half),v.x==0?dx:1,std::min(1.f,v.y+half)};}
  result[i]=r;
 }return result;
}
inline int cornerSide(int number,const std::array<int,4>&c){if(number<c[0]||number>c[3])return 3;if(number<=c[1])return 2;if(number<=c[2])return 1;return 0;}
