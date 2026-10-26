#pragma once
#include "CornerLayout.h"
#include <cstdint>
#include <vector>
struct GaussianRegion {uint32_t x0,y0,x1,y1;float cx,cy;uint32_t kind,pad;};
// kind: 0 = depth along X, 1 = depth along Y, 2 = corner quarter disk.
inline std::array<GaussianRegion,45> gaussianRegions(const std::array<int,4>&corners,bool calibrated,
 const std::array<int,4>&counts,float depth,uint32_t width,uint32_t height){
 std::array<SampleRect,45> rects{};std::array<LedPoint,45> points{};std::array<int,45> sides{};
 float radius=std::min(float(height)*depth,float(std::min(width,height))),dx=radius/width,dy=radius/height;
 if(calibrated){rects=cornerRegions(corners,depth,float(width)/height);points=cornerPoints(corners);for(int i=0;i<45;i++)sides[i]=cornerSide(i+1,corners);}
 else {int index=0;for(int side=0;side<4;side++)for(int k=0;k<counts[side];k++){float a=float(k)/counts[side],b=float(k+1)/counts[side];sides[index]=side;
  if(side==0){rects[index]={0,1-b,dx,1-a};points[index]={0,1-(a+b)*.5f};}
  if(side==1){rects[index]={a,0,b,dy};points[index]={(a+b)*.5f,0};}
  if(side==2){rects[index]={1-dx,a,1,b};points[index]={1,(a+b)*.5f};}
  if(side==3){rects[index]={1-b,1-dy,1-a,1};points[index]={1-(a+b)*.5f,1};}index++;}}
 std::array<GaussianRegion,45> out{};
 for(int i=0;i<45;i++){auto r=rects[i];auto p=points[i];bool corner=calibrated&&std::find(corners.begin(),corners.end(),i+1)!=corners.end();int side=sides[i];
  if(corner)r={p.x==0?0:1-dx,p.y==0?0:1-dy,p.x==0?dx:1,p.y==0?dy:1};
  else if(side==0){r.x0=0;r.x1=dx;}else if(side==2){r.x0=1-dx;r.x1=1;}else if(side==1){r.y0=0;r.y1=dy;}else {r.y0=1-dy;r.y1=1;}
  // Shared along-edge boundaries use the same floor rounding, avoiding overlap.
  auto x0=std::min(width-1,uint32_t(std::max(0.f,std::floor(r.x0*width))));auto y0=std::min(height-1,uint32_t(std::max(0.f,std::floor(r.y0*height))));
  auto x1=std::clamp(uint32_t(r.x1*width),x0+1,width),y1=std::clamp(uint32_t(r.y1*height),y0+1,height);
  out[i]={x0,y0,x1,y1,p.x*width,p.y*height,corner?2u:(side==0||side==2?0u:1u),0};
 }return out;
}
inline std::vector<float> gaussianWeights(float radius){std::vector<float>w(size_t(std::ceil(radius))+1);for(size_t i=0;i<w.size();i++){float d=(i+.5f)/radius;w[i]=d<=1?std::exp(-2*d*d):0;}return w;}
inline constexpr const char* edgeGaussianShader=R"(
Texture2D<float4> source:register(t0);
struct Region {uint4 box;float2 center;uint kind;uint pad;};
StructuredBuffer<Region> regions:register(t1);
StructuredBuffer<float> weights:register(t2);
RWStructuredBuffer<float4> colors:register(u0);
cbuffer Params:register(b0){float hdrScale;float radius;uint2 padding;};
groupshared float4 sums[256];
float3 linearize(float3 v){if(hdrScale>0)return max(v,0)/hdrScale;v=max(v,0);return lerp(v/12.92,pow((v+.055)/1.055,2.4),step(.04045,v));}
[numthreads(256,1,1)]void main(uint3 group:SV_GroupID,uint lane:SV_GroupIndex){
 Region r=regions[group.x];uint rw=r.box.z-r.box.x,count=rw*(r.box.w-r.box.y);float4 sum=0;
 for(uint i=lane;i<count;i+=256){uint2 p=r.box.xy+uint2(i%rw,i/rw);float2 delta=abs(float2(p)+.5-r.center);float w,q;
  if(r.kind==2){q=dot(delta,delta)/(radius*radius);if(q>=1)continue;w=weights[(uint)delta.x]*weights[(uint)delta.y];}
  else {float d=r.kind==0?delta.x:delta.y;q=d*d/(radius*radius);if(q>=1)continue;w=weights[(uint)d];}
  // Compact Gaussian taper: value and slope both reach zero at the boundary.
  float taper=1-q;w*=taper*taper;
  sum+=float4(linearize(source.Load(int3(p,0)).rgb)*w,w);
 }
 sums[lane]=sum;GroupMemoryBarrierWithGroupSync();for(uint step=128;step>0;step/=2){if(lane<step)sums[lane]+=sums[lane+step];GroupMemoryBarrierWithGroupSync();}
 if(lane==0){float3 c=sums[0].rgb/max(sums[0].a,1e-10);c/=max(1,max(c.r,max(c.g,c.b)));colors[group.x]=float4(c,1);}
})";

