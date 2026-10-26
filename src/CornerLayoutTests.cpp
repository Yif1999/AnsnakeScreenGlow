#include "CornerLayout.h"
#include <cassert>
#include <cstdio>
int main(){
 assert(!validCorners({0,14,31,41}));assert(!validCorners({4,31,14,41}));assert(!validCorners({10,20,25,35}));
 unsigned layouts=0;
 for(int a=1;a<=42;a++)for(int b=a+1;b<=43;b++)for(int c=b+1;c<=44;c++)for(int d=c+1;d<=45;d++){
  std::array<int,4> corners{a,b,c,d};if(!validCorners(corners))continue;layouts++;
  auto p=cornerPoints(corners);assert(p[a-1].x==1&&p[a-1].y==1);assert(p[b-1].x==1&&p[b-1].y==0);assert(p[c-1].x==0&&p[c-1].y==0);assert(p[d-1].x==0&&p[d-1].y==1);assert(p[0].x>p[44].x);
  auto regions=cornerRegions(corners,.12f,16.f/9);
  for(int i=0;i<45;i++){assert(p[i].x>=0&&p[i].x<=1&&p[i].y>=0&&p[i].y<=1);auto r=regions[i];assert(r.x0>=0&&r.y0>=0&&r.x1<=1&&r.y1<=1&&r.x1>r.x0&&r.y1>r.y0);assert(cornerSide(i+1,corners)>=0&&cornerSide(i+1,corners)<=3);}
  for(float aspect:{16.f/9,9.f/16,32.f/9})for(float depth:{.02f,.12f,.3f}){auto boxes=cornerRegions(corners,depth,aspect);auto base=cornerRegions(corners,.02f,aspect);for(int i=0;i<45;i++){auto r=boxes[i];int n=i+1;
   assert(r.x0>=0&&r.y0>=0&&r.x1<=1.000001f&&r.y1<=1.000001f);
   if(std::find(corners.begin(),corners.end(),n)!=corners.end())continue;
   bool horizontal=n<a||n>d||(n>b&&n<c);
   if(horizontal){assert(std::abs((r.x1-r.x0)-(base[i].x1-base[i].x0))<.000001f);assert(std::abs(r.y1-r.y0-std::min(.45f,depth))<.000001f);}
   else {assert(std::abs((r.y1-r.y0)-(base[i].y1-base[i].y0))<.000001f);assert(std::abs(r.x1-r.x0-std::min(.45f,depth/aspect))<.000001f);}
   if(i<44&&std::find(corners.begin(),corners.end(),n+1)==corners.end()&&cornerSide(n,corners)==cornerSide(n+1,corners)){
    auto q=boxes[i+1];float overlapX=std::min(r.x1,q.x1)-std::max(r.x0,q.x0),overlapY=std::min(r.y1,q.y1)-std::max(r.y0,q.y0);assert(overlapX<.000001f||overlapY<.000001f);
   }
  }}
 }
 std::printf("PASS %u layouts: bounded regions, fixed along-edge width, inward depth, ordinary same-edge neighbors do not overlap; 3 aspect ratios x 3 depths\n",layouts);
}
