#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

namespace ledcolor {
using RGB=std::array<float,3>;
struct VisualTrim {RGB gain{1,1,1};float darkness=1;};
inline void validateTrim(const VisualTrim&t){for(float v:t.gain)if(!std::isfinite(v)||v<.5f||v>1.5f)throw std::runtime_error("Invalid visual gain");if(!std::isfinite(t.darkness)||t.darkness<.5f||t.darkness>2)throw std::runtime_error("Invalid visual curve");}
inline VisualTrim readTrim(std::istream&in){std::string magic,extra;int version;VisualTrim t;if(!(in>>magic>>version>>t.gain[0]>>t.gain[1]>>t.gain[2]>>t.darkness)||magic!="ANSNAKE_VISUAL"||version!=1||(in>>extra))throw std::runtime_error("Invalid visual profile");validateTrim(t);return t;}
inline void writeTrim(std::ostream&out,const VisualTrim&t){validateTrim(t);out<<"ANSNAKE_VISUAL 1\n"<<t.gain[0]<<' '<<t.gain[1]<<' '<<t.gain[2]<<' '<<t.darkness<<'\n';}
struct Profile {
 std::array<float,9> matrix{1,0,0,0,1,0,0,0,1};
 std::string response="srgb";
 RGB gamma{1,1,1};
 std::array<std::array<float,17>,3> measured{};
};
inline void validate(const Profile&p){
 for(float v:p.matrix)if(!std::isfinite(v)||std::abs(v)>16)throw std::runtime_error("Invalid color matrix");
 if(p.response=="srgb"||p.response=="linear")return;
 if(p.response=="power"){for(float v:p.gamma)if(!std::isfinite(v)||v<.1f||v>5)throw std::runtime_error("Invalid device gamma");return;}
 if(p.response!="measured")throw std::runtime_error("Unknown LED response");
 for(auto row:p.measured){if(row.front()!=0||row.back()!=1)throw std::runtime_error("Response endpoints must be 0 and 1");for(int i=0;i<17;i++)if(!std::isfinite(row[i])||row[i]<0||row[i]>1||(i&&row[i]<=row[i-1]))throw std::runtime_error("Response must increase strictly");}
}
// Strict format: version, 9 row-major coefficients, response, response data.
inline Profile read(std::istream&in){Profile p;std::string magic;int version=0;if(!(in>>magic>>version))throw std::runtime_error("Missing profile header");
 if(magic!="ANSNAKE_LED"||version!=1)throw std::runtime_error("Unsupported profile version");
 for(float&v:p.matrix)if(!(in>>v))throw std::runtime_error("Missing matrix");
 if(!(in>>p.response))throw std::runtime_error("Missing response");
 if(p.response=="power")for(float&v:p.gamma)if(!(in>>v))throw std::runtime_error("Missing gamma");
 if(p.response=="measured")for(auto&row:p.measured)for(float&v:row)if(!(in>>v))throw std::runtime_error("Missing measured response");
 std::string extra;if(in>>extra)throw std::runtime_error("Unexpected profile data");validate(p);return p;
}
inline float inverse(const Profile&p,int channel,float x){
 if(x<=0)return 0;if(x>=1)return 1;
 if(p.response=="srgb")return x<=.0031308f?12.92f*x:1.055f*std::pow(x,1.f/2.4f)-.055f;
 if(p.response=="linear")return x;
 if(p.response=="power")return std::pow(x,1.f/p.gamma[channel]);
 const auto&r=p.measured[channel];auto it=std::lower_bound(r.begin()+1,r.end(),x);int i=int(it-r.begin());return (i-1+(x-r[i-1])/(r[i]-r[i-1]))/16.f;
}
class Encoder {
 Profile profile;
 std::array<std::array<float,255>,3> thresholds{};
public:
 explicit Encoder(const Profile&p={},const VisualTrim&trim={}):profile(p){validate(p);validateTrim(trim);for(int c=0;c<3;c++)for(int j=0;j<3;j++)profile.matrix[c*3+j]*=trim.gain[c];for(int c=0;c<3;c++)for(int i=0;i<255;i++){
   float q=(i+.5f)/255.f,value=q;
   if(p.response=="srgb")value=q<=.04045f?q/12.92f:std::pow((q+.055f)/1.055f,2.4f);
   else if(p.response=="power")value=std::pow(q,p.gamma[c]);
   else if(p.response=="measured"){float pos=q*16;int k=std::min(15,int(pos));value=p.measured[c][k]+(p.measured[c][k+1]-p.measured[c][k])*(pos-k);}
   thresholds[c][i]=std::pow(value,1.f/trim.darkness);
  }}
 std::array<uint8_t,3> encode(RGB rgb,float brightness)const{
  for(float&v:rgb)if(!std::isfinite(v))v=0;
  brightness=std::isfinite(brightness)?std::clamp(brightness,0.f,1.f):0;
  std::array<uint8_t,3> out{};
  for(int c=0;c<3;c++){
   float x=(profile.matrix[c*3]*rgb[0]+profile.matrix[c*3+1]*rgb[1]+profile.matrix[c*3+2]*rgb[2])*brightness;
   x=std::isfinite(x)?std::clamp(x,0.f,1.f):0;
   out[c]=uint8_t(std::upper_bound(thresholds[c].begin(),thresholds[c].end(),x)-thresholds[c].begin());
  }return out;
 }
};
}


