#include "LedColor.h"
#include <sstream>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <limits>
int main(){using namespace ledcolor;
 VisualTrim trim;trim.gain={.8f,1.f,1.1f};trim.darkness=1.3f;std::stringstream persisted;writeTrim(persisted,trim);auto restored=readTrim(persisted);assert(restored.gain==trim.gain&&restored.darkness==trim.darkness);
 Profile linear;linear.response="linear";Encoder visual(linear,trim);auto corrected=visual.encode({.5f,.5f,.5f},1);for(int c=0;c<3;c++)assert(std::abs(int(corrected[c])-int(std::lround(255*std::pow(.5f*trim.gain[c],trim.darkness))))<=1);
 for(auto text:{"ANSNAKE_VISUAL 1 1 1 1 0", "ANSNAKE_VISUAL 1 2 1 1 1", "ANSNAKE_VISUAL 1 1 1 1 1 extra"}){bool failed=false;try{std::istringstream in(text);readTrim(in);}catch(...){failed=true;}assert(failed);}
 Profile p;Encoder legacy(p);int error=0;
 for(int i=0;i<=100000;i++){float x=i/100000.f;int expected=int(std::lround(inverse(p,0,x)*255));auto v=legacy.encode({x,x,x},1);error=std::max(error,std::abs(int(v[0])-expected));assert(v[0]==v[1]&&v[1]==v[2]);}assert(error<=1);
 p.response="linear";p.matrix={0,1,0,0,0,1,1,0,0};Encoder swapped(p);assert((swapped.encode({1,0,0},1)==std::array<uint8_t,3>{0,0,255}));assert((swapped.encode({1,1,1},0)==std::array<uint8_t,3>{0,0,0}));
 p.matrix={1,0,0,0,1,0,0,0,1};p.response="power";p.gamma={2,1,.5f};Encoder power(p);auto v=power.encode({.25f,.25f,.25f},1);assert(v[0]==128&&v[1]==64&&v[2]==16);
 p.response="measured";for(int c=0;c<3;c++)for(int i=0;i<=16;i++)p.measured[c][i]=std::pow(i/16.f,float(c+1));Encoder measured(p);
 for(int c=0;c<3;c++)for(int i=0;i<=16;i++){RGB rgb{};rgb[c]=p.measured[c][i];assert(std::abs(int(measured.encode(rgb,1)[c])-int(std::lround(i*255/16.f)))<=1);}
 auto bad=measured.encode({std::numeric_limits<float>::quiet_NaN(),-10,100},1);assert(bad[0]==0&&bad[1]==0&&bad[2]==255);
 std::istringstream valid("ANSNAKE_LED 1 1 0 0 0 1 0 0 0 1 power 1 2 3");assert(read(valid).gamma[2]==3);
 for(auto text:{"", "ANSNAKE_LED 2", "ANSNAKE_LED 1 1 0 0 0 1 0 0 0 1 unknown", "ANSNAKE_LED 1 1 0 0 0 1 0 0 0 1 power 0 1 1", "ANSNAKE_LED 1 1 0 0 0 1 0 0 0 1 linear trailing"}){bool rejected=false;try{std::istringstream in(text);read(in);}catch(...){rejected=true;}assert(rejected);}
 p.measured[0][1]=0;bool rejected=false;try{Encoder invalid(p);}catch(...){rejected=true;}assert(rejected);
 auto start=std::chrono::steady_clock::now();uint64_t checksum=0;for(int frame=0;frame<100000;frame++)for(int i=0;i<45;i++){float x=((frame+i)%4097)/4096.f;auto out=measured.encode({x,x*.5f,x*.25f},.4f);checksum+=out[0]+out[1]+out[2];}
 double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()/100000;
 std::printf("PASS: compatibility <= %d byte, channel mapping, black, inverse response, measured points, invalid inputs/profiles\n45 LEDs CPU microbenchmark: %.3f us/frame, checksum=%llu; LUT=3060 bytes, no GPU allocation\n",error,us,(unsigned long long)checksum);
}

