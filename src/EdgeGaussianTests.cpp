#include "CaptureCommon.h"
#include "EdgeGaussian.h"
#include <cassert>
#include <cmath>
int main(){try{
 com_ptr<ID3D11Device>d;com_ptr<ID3D11DeviceContext>c;check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,d.put(),nullptr,c.put()));auto cs=shader(d.get(),edgeGaussianShader);
 for(int test=0;test<10;test++){
  UINT width=test>=8?3840:384,height=test>=8?2160:216;float radius=height*(test%2?.3f:.12f),hdr=test==7?0.f:1.f;
  auto regions=gaussianRegions({4,14,32,42},true,{10,18,11,6},radius/height,width,height);auto weights=gaussianWeights(radius);
  std::vector<std::array<float,4>> pixels(size_t(width)*height);
  for(UINT y=0;y<height;y++)for(UINT x=0;x<width;x++){float v=.4f;if(test==1)v=float(x)/width;if(test==2)v=float(y)/height;if(test>=3&&test<=6)v=(x%31==test&&y%23==test)?1.f:0.f;if(test==7)v=((x+y)%2)?1.f:0.f;if(test==8)v=float(y)/height;if(test==9)v=(x%101==0&&y%97==0)?2.f:0.f;pixels[size_t(y)*width+x]={v,v*.7f,v*.3f,1};}
  D3D11_TEXTURE2D_DESC td{};td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;D3D11_SUBRESOURCE_DATA data{pixels.data(),width*16,0};com_ptr<ID3D11Texture2D>tex;check_hresult(d->CreateTexture2D(&td,&data,tex.put()));com_ptr<ID3D11ShaderResourceView>input;check_hresult(d->CreateShaderResourceView(tex.get(),nullptr,input.put()));
  auto buffer=[&](UINT bytes,UINT stride,const void*data,UINT bind){D3D11_BUFFER_DESC desc{};desc.ByteWidth=bytes;desc.BindFlags=bind;if(stride){desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;desc.StructureByteStride=stride;}D3D11_SUBRESOURCE_DATA initial{data,0,0};com_ptr<ID3D11Buffer>b;check_hresult(d->CreateBuffer(&desc,data?&initial:nullptr,b.put()));return b;};
  auto rb=buffer(sizeof regions,sizeof(GaussianRegion),regions.data(),D3D11_BIND_SHADER_RESOURCE);auto wb=buffer(UINT(weights.size()*4),4,weights.data(),D3D11_BIND_SHADER_RESOURCE);auto output=buffer(45*16,16,nullptr,D3D11_BIND_UNORDERED_ACCESS);struct {float hdr,radius;UINT pad[2];}params{hdr,radius,{}};auto cb=buffer(sizeof params,0,&params,D3D11_BIND_CONSTANT_BUFFER);
  com_ptr<ID3D11ShaderResourceView>rv,wv;check_hresult(d->CreateShaderResourceView(rb.get(),nullptr,rv.put()));check_hresult(d->CreateShaderResourceView(wb.get(),nullptr,wv.put()));com_ptr<ID3D11UnorderedAccessView>uav;check_hresult(d->CreateUnorderedAccessView(output.get(),nullptr,uav.put()));D3D11_BUFFER_DESC read{};read.ByteWidth=45*16;read.Usage=D3D11_USAGE_STAGING;read.CPUAccessFlags=D3D11_CPU_ACCESS_READ;com_ptr<ID3D11Buffer>staging;check_hresult(d->CreateBuffer(&read,nullptr,staging.put()));
  ID3D11ShaderResourceView*views[]={input.get(),rv.get(),wv.get()};auto constant=cb.get();auto target=uav.get();c->CSSetShader(cs.get(),nullptr,0);c->CSSetShaderResources(0,3,views);c->CSSetConstantBuffers(0,1,&constant);c->CSSetUnorderedAccessViews(0,1,&target,nullptr);c->Dispatch(45,1,1);c->CopyResource(staging.get(),output.get());D3D11_MAPPED_SUBRESOURCE map{};check_hresult(c->Map(staging.get(),0,D3D11_MAP_READ,0,&map));auto actual=static_cast<float*>(map.pData);double maxError=0;
  for(int led=0;led<45;led++){auto r=regions[led];double sum[3]{},total=0;for(UINT y=r.y0;y<r.y1;y++)for(UINT x=r.x0;x<r.x1;x++){
   double dx=(x+.5-r.cx)/radius,dy=(y+.5-r.cy)/radius,dist=r.kind==2?dx*dx+dy*dy:r.kind==0?dx*dx:dy*dy;if(dist>1)continue;double weight=std::exp(-2*dist);auto pixel=pixels[size_t(y)*width+x];for(int ch=0;ch<3;ch++){double v=pixel[ch];if(hdr==0)v=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);sum[ch]+=v*weight;}total+=weight;
   }double scale=std::max({1.,sum[0]/total,sum[1]/total,sum[2]/total});for(int ch=0;ch<3;ch++){assert(std::isfinite(actual[led*4+ch]));maxError=std::max(maxError,std::abs(actual[led*4+ch]-sum[ch]/total/scale));}}
  c->Unmap(staging.get(),0);std::printf("case %d %ux%u CPU/GPU max error %.9f\n",test,width,height,maxError);if(maxError>.0001)return 1;
 }
 // Analytic geometry properties independent of GPU and summed-color agreement.
 auto r=gaussianRegions({4,14,32,42},true,{10,18,11,6},.15f,3840,2160);assert(r[13].kind==2&&r[13].cx==3840&&r[13].cy==0);assert(r[20].kind==1&&r[20].cy==0);assert(r[6].kind==0&&r[6].cx==3840);
 auto w=gaussianWeights(324);assert(w.front()>w.back());for(size_t i=1;i<w.size();i++)assert(w[i]<=w[i-1]);
 std::puts("PASS exact edge-centered depth Gaussian + corner radial disk (CPU reference, gradients, sparse HDR/SDR, 4K)");return 0;
 }catch(hresult_error const&e){std::printf("FAIL %ls\n",e.message().c_str());return 1;}}
