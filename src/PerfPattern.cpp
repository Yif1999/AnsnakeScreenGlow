#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <string>
#include <algorithm>
using Microsoft::WRL::ComPtr;
bool running=true;int tick=0;bool dynamicPattern=true;
double clockMs(){static auto t=std::chrono::steady_clock::now();return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();}
void paint(HWND h){HDC dc=GetDC(h);RECT r;GetClientRect(h,&r);for(int band=0;band<8;band++){double phase=(dynamicPattern?tick*.045:0)+band*.7;int red=int(90+65*std::sin(phase)),green=int(90+65*std::sin(phase+2.094)),blue=int(90+65*std::sin(phase+4.189));SetDCBrushColor(dc,RGB(red,green,blue));RECT stripe{r.right*band/8,0,r.right*(band+1)/8,r.bottom};FillRect(dc,&stripe,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));}ReleaseDC(h,dc);}
LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){if(m==WM_CLOSE){running=false;return 0;}if(m==WM_KEYDOWN&&w==VK_ESCAPE){running=false;return 0;}if(m==WM_PAINT){PAINTSTRUCT p;BeginPaint(h,&p);EndPaint(h,&p);paint(h);return 0;}if(m==WM_ERASEBKGND)return 1;return DefWindowProc(h,m,w,l);}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR cmd,int){
 SetProcessDPIAware();dynamicPattern=wcsstr(cmd,L"--static")==nullptr;int seconds=32;auto arg=wcsstr(cmd,L"--seconds=");if(arg)seconds=std::clamp(_wtoi(arg+10),5,120);
 wchar_t exe[MAX_PATH];GetModuleFileNameW(nullptr,exe,MAX_PATH);std::wstring dir=exe;dir=dir.substr(0,dir.find_last_of(L"\\")+1);
 {std::ofstream f(dir+L"gpu-adapters.txt");ComPtr<IDXGIFactory1>factory;if(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))for(UINT i=0;;i++){ComPtr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,&a)==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);char name[256];WideCharToMultiByte(CP_UTF8,0,d.Description,-1,name,sizeof name,nullptr,nullptr);char id[64];sprintf_s(id,"luid_0x%08x_0x%08x",unsigned(d.AdapterLuid.HighPart),unsigned(d.AdapterLuid.LowPart));f<<id<<"="<<name<<"\n";}}
 MONITORINFO info{sizeof info};GetMonitorInfo(MonitorFromPoint(POINT{0,0},MONITOR_DEFAULTTOPRIMARY),&info);auto r=info.rcMonitor;
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"ScreenGlowPerfPattern";RegisterClassW(&wc);HWND h=CreateWindowW(wc.lpszClassName,L"屏幕灯性能测试（Esc 退出）",WS_POPUP,r.left,r.top,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,nullptr);if(!h)return 1;
 ShowWindow(h,SW_SHOW);SetForegroundWindow(h);HANDLE timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);if(!timer){DestroyWindow(h);return 2;}double start=clockMs(),next=start;int missed=0;paint(h);
 while(running&&clockMs()-start<seconds*1000){MSG msg;while(PeekMessage(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}double now=clockMs();if(now>=next){if(dynamicPattern){paint(h);tick++;}next+=1000./60.;if(now-next>1000./60.){int skip=int((now-next)/(1000./60.));next+=skip*1000./60.;missed+=skip;}}
  LARGE_INTEGER due;due.QuadPart=-std::max<LONGLONG>(1,LONGLONG((next-clockMs())*10000));SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE);MsgWaitForMultipleObjects(1,&timer,FALSE,100,QS_ALLINPUT);
 }
 std::ofstream report(dir+L"pattern-results.txt");report<<"dynamic="<<dynamicPattern<<"\npainted_frames="<<tick<<"\nelapsed_seconds="<<(clockMs()-start)/1000<<"\nmissed_slots="<<missed<<"\n";CloseHandle(timer);DestroyWindow(h);return 0;
}
