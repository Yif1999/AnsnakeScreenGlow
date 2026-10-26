#include "CaptureCommon.h"
#include <fstream>
#include <string>
const char* loadShader=R"(
RWTexture2D<float4> result:register(u0);cbuffer Work:register(b0){uint iterations;uint tick;uint2 pad;};
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){if(id.x>=1920||id.y>=1080)return;float4 v=float4(id.xy*.001,tick*.001,.73);
[loop]for(uint i=0;i<iterations;i++){v=sin(v.wxyz*float4(1.13,1.17,1.19,1.23)+v*.31+.00001*i);}
result[id.xy]=v;})";
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR cmd,int){try{
 auto arg=[&](const wchar_t*key,int value){auto p=wcsstr(cmd,key);return p?_wtoi(p+wcslen(key)):value;};int seconds=std::clamp(arg(L"--seconds=",35),5,90),iterations=std::clamp(arg(L"--iterations=",256),16,1024);
 std::wstring path=L"build/gpu-load.csv";if(auto p=wcsstr(cmd,L"--output=")){p+=9;path=std::wstring(p,wcscspn(p,L" \t"));}std::ofstream csv(path);csv<<"end_ms,wall_ms,gpu_ms\n";
 com_ptr<IDXGIFactory1>factory;check_hresult(CreateDXGIFactory1(__uuidof(IDXGIFactory1),factory.put_void()));com_ptr<IDXGIAdapter1>adapter;for(UINT i=0;;i++){com_ptr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,a.put())==DXGI_ERROR_NOT_FOUND)throw hresult_error(E_FAIL);DXGI_ADAPTER_DESC1 desc{};a->GetDesc1(&desc);if(desc.VendorId==0x10de){adapter=a;break;}}
 com_ptr<ID3D11Device>d;com_ptr<ID3D11DeviceContext>c;check_hresult(D3D11CreateDevice(adapter.get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,d.put(),nullptr,c.put()));auto shaderObject=shader(d.get(),loadShader);c->CSSetShader(shaderObject.get(),nullptr,0);
 D3D11_TEXTURE2D_DESC td{};td.Width=1920;td.Height=1080;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;td.BindFlags=D3D11_BIND_UNORDERED_ACCESS;com_ptr<ID3D11Texture2D>texture;com_ptr<ID3D11UnorderedAccessView>view;check_hresult(d->CreateTexture2D(&td,nullptr,texture.put()));check_hresult(d->CreateUnorderedAccessView(texture.get(),nullptr,view.put()));auto uav=view.get();c->CSSetUnorderedAccessViews(0,1,&uav,nullptr);
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;com_ptr<ID3D11Buffer>cb;check_hresult(d->CreateBuffer(&bd,nullptr,cb.put()));auto constant=cb.get();c->CSSetConstantBuffers(0,1,&constant);
 com_ptr<ID3D11Query>start,end,disjoint,complete;D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP,0};check_hresult(d->CreateQuery(&q,start.put()));check_hresult(d->CreateQuery(&q,end.put()));q.Query=D3D11_QUERY_TIMESTAMP_DISJOINT;check_hresult(d->CreateQuery(&q,disjoint.put()));q.Query=D3D11_QUERY_EVENT;check_hresult(d->CreateQuery(&q,complete.put()));
 HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);if(!timer)throw hresult_error(E_FAIL);double deadline=ms()+seconds*1000;UINT tick=0;
 while(ms()<deadline){double begin=ms();UINT params[]={UINT(iterations),tick++,0,0};c->UpdateSubresource(cb.get(),0,nullptr,params,0,0);c->Begin(disjoint.get());c->End(start.get());for(int batch=0;batch<8;batch++)c->Dispatch(240,135,1);c->End(end.get());c->End(disjoint.get());c->End(complete.get());c->Flush();
  for(;;){HRESULT status=c->GetData(complete.get(),nullptr,0,D3D11_ASYNC_GETDATA_DONOTFLUSH);if(status==S_OK)break;check_hresult(status);if(ms()-begin>3000)throw hresult_error(E_FAIL);LARGE_INTEGER due{};due.QuadPart=-1000;SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);WaitForSingleObject(timer,100);}
  UINT64 a=0,b=0;D3D11_QUERY_DATA_TIMESTAMP_DISJOINT info{};check_hresult(c->GetData(disjoint.get(),&info,sizeof info,0));check_hresult(c->GetData(start.get(),&a,sizeof a,0));check_hresult(c->GetData(end.get(),&b,sizeof b,0));csv<<ms()<<','<<ms()-begin<<','<<(info.Disjoint?-1:double(b-a)*1000/info.Frequency)<<'\n';
 }CloseHandle(timer);return 0;
 }catch(...){return 2;}}
