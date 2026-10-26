#include "CaptureCommon.h"

#include "CornerLayout.h"
#include "LedColor.h"
#include <filesystem>
#include <memory>
#include "EdgeGaussian.h"


#include <commctrl.h>
#include <shellapi.h>
#include <setupapi.h>
#include <devguid.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <array>
#include <string>
#include <cmath>
#include <fstream>
#include <winrt/Windows.Security.Authorization.AppCapabilityAccess.h>

struct Config {std::array<int,4> corners{};bool calibrated=false;int brightness=40,depth=12,left=12,top=21,right=12,bottom=0,reverse=0,offset=0,screen=0,mode=0;};
struct State {
 bool benchmark=false,captureOnly=false,fixedSendClock=false;int captureBuffers=2;
 std::atomic<bool> identifying{false},restoreAfterIdentify{false};std::atomic<int> identifyLed{0};bool rawHardware=false;int gpuPath=1;  // 0 Intel WGC/compute; 1 NVIDIA WGC/compute; 2/3 NVIDIA -> CPU -> Intel, edges/full.
 std::vector<double> transferSamples;
 std::vector<double> compositorAges;
 double compositorAt=0;
 HANDLE colorsReady=nullptr;
 uint64_t colorSequence=0;
 double readyAt=0;
 std::vector<double> freshAges,readyAges;
 
 std::vector<double> processSamples,sendIntervals,writeSamples,internalAges;
 double sourceAt=0,lastSendAt=0;
 std::atomic<bool> run{true},enabled{true};std::atomic<unsigned> revision{0};
 std::mutex lock;Config config;std::array<float,180> colors{};bool hasColors=false;
 std::wstring captureStatus=L"正在初始化独显…",serialStatus=L"正在查找灯带…";
 uint64_t captured=0,sent=0,replies=0;double processMs=0,maxProcessMs=0,writeMs=0,maxWriteMs=0,intelMiB=0,nvidiaMiB=0;
} g;
std::wstring iniPath;
std::vector<HMONITOR> monitors;
HWND windowHandle,statusLabel,enableButton,brightnessSlider,depthSlider,reverseBox,modeCombo,screenCombo,offsetEdit;
HWND depthValue;HWND sideEdits[4];HFONT fontHandle;NOTIFYICONDATAW tray{};
Config config(){std::lock_guard guard(g.lock);return g.config;}
void captureStatus(std::wstring s){std::lock_guard guard(g.lock);g.captureStatus=std::move(s);}
void serialStatus(std::wstring s){std::lock_guard guard(g.lock);g.serialStatus=std::move(s);}
struct Handle {HANDLE h=nullptr;Handle()=default;explicit Handle(HANDLE v):h(v){}~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}Handle(Handle const&)=delete;};
void waitFor(HANDLE timer,double deadline){double left=deadline-ms();if(left<=0)return;LARGE_INTEGER due{};due.QuadPart=-std::max<LONGLONG>(1,LONGLONG(left*10000));if(SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE))WaitForSingleObject(timer,1000);}
double qpcMs(){static const double frequency=[](){LARGE_INTEGER v{};QueryPerformanceFrequency(&v);return double(v.QuadPart);}();LARGE_INTEGER now{};QueryPerformanceCounter(&now);return now.QuadPart*1000./frequency;}

void requireBorderless(GraphicsCaptureSession const& session){
 // Use the documented consent API once per process. Never work around a denial
 // or silently start a bordered session when the user requires no border.
 static const bool allowed=[](){
  try{return GraphicsCaptureAccess::RequestAccessAsync(GraphicsCaptureAccessKind::Borderless).get()==winrt::Windows::Security::Authorization::AppCapabilityAccess::AppCapabilityAccessStatus::Allowed;}
  catch(hresult_error const&){return false;}
 }();
 if(!allowed)throw hresult_error(E_ACCESSDENIED,L"系统未允许无边框捕获；已停止采集，不回退到黄色边框模式。");
 auto border=session.try_as<IGraphicsCaptureSession3>();
 if(!border)throw hresult_error(E_NOTIMPL,L"此 Windows 版本不支持无边框捕获，已停止采集。");
 border.IsBorderRequired(false);
 if(border.IsBorderRequired())throw hresult_error(E_FAIL,L"无法应用无边框设置，已停止采集。");
}

float whiteLevel(HMONITOR monitor){
 MONITORINFOEXW info{};info.cbSize=sizeof info;GetMonitorInfoW(monitor,&info);
 UINT np=0,nm=0;check_hresult(HRESULT_FROM_WIN32(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&np,&nm)));
 std::vector<DISPLAYCONFIG_PATH_INFO> paths(np);std::vector<DISPLAYCONFIG_MODE_INFO> modes(nm);
 check_hresult(HRESULT_FROM_WIN32(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&np,paths.data(),&nm,modes.data(),nullptr)));
 for(UINT i=0;i<np;i++){auto &p=paths[i];DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};name.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof name,p.sourceInfo.adapterId,p.sourceInfo.id};
  if(DisplayConfigGetDeviceInfo(&name.header)==ERROR_SUCCESS&&wcscmp(name.viewGdiDeviceName,info.szDevice)==0){DISPLAYCONFIG_SDR_WHITE_LEVEL level{};level.header={DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL,sizeof level,p.targetInfo.adapterId,p.targetInfo.id};
   check_hresult(HRESULT_FROM_WIN32(DisplayConfigGetDeviceInfo(&level.header)));return std::max(0.01f,level.SDRWhiteLevel/1000.f);}}
 throw hresult_error(E_FAIL,L"无法读取 HDR 的 SDR 白电平，暂停取色。");
}
void captureSession(){
 unsigned revision=g.revision.load();Config cfg=config();

 if(g.identifying){captureStatus(L"单灯定位中 · 采集已暂停");Sleep(100);return;}
 if(cfg.mode!=0){captureStatus(cfg.mode==1?L"静态暖色 · 屏幕采集已停止":L"布局校准 · 屏幕采集已停止");Sleep(200);return;}
 if(!g.enabled){captureStatus(L"已暂停 · 屏幕采集已停止");Sleep(200);return;}
 com_ptr<IDXGIFactory1>factory;check_hresult(CreateDXGIFactory1(__uuidof(IDXGIFactory1),factory.put_void()));
 bool hdr=false;com_ptr<IDXGIAdapter1>intel,displayAdapter;com_ptr<IDXGIAdapter3>nvidia;HMONITOR monitor=monitors.at(std::min<size_t>(cfg.screen,monitors.size()-1));
 for(UINT i=0;;i++){com_ptr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,a.put())==DXGI_ERROR_NOT_FOUND)break;DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);if(d.VendorId==0x8086&&d.DeviceId==0x4680)intel=a;if(d.VendorId==0x10de&&!nvidia)nvidia=a.try_as<IDXGIAdapter3>();
  for(UINT j=0;;j++){com_ptr<IDXGIOutput>o;if(a->EnumOutputs(j,o.put())==DXGI_ERROR_NOT_FOUND)break;DXGI_OUTPUT_DESC od{};o->GetDesc(&od);if(od.Monitor==monitor){displayAdapter=a;auto o6=o.try_as<IDXGIOutput6>();if(o6){DXGI_OUTPUT_DESC1 od1{};o6->GetDesc1(&od1);if(od1.ColorSpace==DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)hdr=true;}}}}

 DXGI_ADAPTER_DESC1 adapterDesc{};if(!displayAdapter)throw hresult_error(E_FAIL,L"未找到屏幕显卡");displayAdapter->GetDesc1(&adapterDesc);if(adapterDesc.VendorId!=0x10de)throw hresult_error(E_FAIL,L"正式版要求所选屏幕连接 NVIDIA 显卡");nvidia=displayAdapter.as<IDXGIAdapter3>();
 com_ptr<ID3D11Device>device;com_ptr<ID3D11DeviceContext>ctx;
 check_hresult(D3D11CreateDevice(displayAdapter.get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,device.put(),nullptr,ctx.put()));auto captureDevice=device;

 auto dxgi=captureDevice.as<IDXGIDevice>();com_ptr<IInspectable>inspect;check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(),inspect.put()));auto runtimeDevice=inspect.as<IDirect3DDevice>();
 auto interop=get_activation_factory<GraphicsCaptureItem,IGraphicsCaptureItemInterop>();GraphicsCaptureItem item{nullptr};check_hresult(interop->CreateForMonitor(monitor,guid_of<GraphicsCaptureItem>(),put_abi(item)));

 auto size=item.Size();float hdrScale=hdr?whiteLevel(monitor):0;auto direct=shader(device.get(),edgeGaussianShader);
 D3D11_BUFFER_DESC bd{};bd.ByteWidth=45*16;bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;bd.StructureByteStride=16;
 com_ptr<ID3D11Buffer>colors,stage,constants;check_hresult(device->CreateBuffer(&bd,nullptr,colors.put()));com_ptr<ID3D11UnorderedAccessView>colorUav;check_hresult(device->CreateUnorderedAccessView(colors.get(),nullptr,colorUav.put()));
 bd.BindFlags=0;bd.MiscFlags=0;bd.StructureByteStride=0;bd.Usage=D3D11_USAGE_STAGING;bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;check_hresult(device->CreateBuffer(&bd,nullptr,stage.put()));
 float radius=std::min(size.Height*cfg.depth/100.f,float(std::min(size.Width,size.Height)));struct {float hdr,radius;UINT pad[2];}dims{hdrScale,radius,{}};
 bd={};bd.ByteWidth=sizeof dims;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA data{&dims,0,0};check_hresult(device->CreateBuffer(&bd,&data,constants.put()));auto constant=constants.get();ctx->CSSetConstantBuffers(0,1,&constant);
 auto rectangles=gaussianRegions(cfg.corners,cfg.calibrated,{cfg.left,cfg.top,cfg.right,cfg.bottom},cfg.depth/100.f,size.Width,size.Height);auto weights=gaussianWeights(radius);
 com_ptr<ID3D11Buffer> regionBuffer,weightBuffer;com_ptr<ID3D11ShaderResourceView>regionView,weightView;
 auto makeBuffer=[&](const void*ptr,UINT bytes,UINT stride,com_ptr<ID3D11Buffer>&buffer,com_ptr<ID3D11ShaderResourceView>&view){D3D11_BUFFER_DESC b{};b.ByteWidth=bytes;b.BindFlags=D3D11_BIND_SHADER_RESOURCE;b.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;b.StructureByteStride=stride;D3D11_SUBRESOURCE_DATA initial{ptr,0,0};check_hresult(device->CreateBuffer(&b,&initial,buffer.put()));check_hresult(device->CreateShaderResourceView(buffer.get(),nullptr,view.put()));};
 makeBuffer(rectangles.data(),sizeof rectangles,sizeof(GaussianRegion),regionBuffer,regionView);makeBuffer(weights.data(),UINT(weights.size()*sizeof(float)),sizeof(float),weightBuffer,weightView);
 ID3D11ShaderResourceView* views[]={regionView.get(),weightView.get()};ctx->CSSetShaderResources(1,2,views);

 auto pool=Direct3D11CaptureFramePool::CreateFreeThreaded(runtimeDevice,hdr?DirectXPixelFormat::R16G16B16A16Float:DirectXPixelFormat::B8G8R8A8UIntNormalized,g.captureBuffers,size);
 auto event=std::make_shared<Handle>(CreateEvent(nullptr,FALSE,FALSE,nullptr));if(!event->h)check_hresult(HRESULT_FROM_WIN32(GetLastError()));
 auto token=pool.FrameArrived([event](auto const&,auto const&){SetEvent(event->h);});auto session=pool.CreateCaptureSession(item);session.IsCursorCaptureEnabled(false);
 requireBorderless(session);
 session.StartCapture();captureStatus(L"NVIDIA · 全像素边缘高斯 · "+std::to_wstring(size.Width)+L"×"+std::to_wstring(size.Height)+(hdr?L" · HDR":L" · SDR")+L" · 无边框");
 double next=0,memoryAt=0;auto intel3=intel?intel.as<IDXGIAdapter3>():com_ptr<IDXGIAdapter3>{};
 // Keep at most the pool's capacity; never retain an unbounded series of frames.
 struct SourceView {com_ptr<ID3D11Texture2D> texture;com_ptr<ID3D11ShaderResourceView> view;};
 std::vector<SourceView> sourceViews;sourceViews.reserve(g.captureBuffers);
 try{
  while(g.run&&g.enabled&&g.revision==revision){
   if(WaitForSingleObject(event->h,100)!=WAIT_OBJECT_0)continue;auto f=pool.TryGetNextFrame();if(!f)continue;
   for(;;){auto newer=pool.TryGetNextFrame();if(!newer)break;f.Close();f=newer;}
   auto actual=f.ContentSize();if(actual.Width!=size.Width||actual.Height!=size.Height){f.Close();break;}
   if(g.captureOnly){f.Close();std::lock_guard guard(g.lock);g.captured++;continue;}
   double begin=ms();if(begin<next-2){f.Close();continue;}next=std::max(next+1000./60.,begin);
   // Diagnostic only: this machine's NVIDIA WGC timestamps can be in the
   // future. Preserve signed deltas, never clamp them into a latency claim.
   double compositorAt=begin-(qpcMs()-f.SystemRelativeTime().count()/10000.);
   auto access=f.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();com_ptr<ID3D11Texture2D>source;check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D),source.put_void()));

   auto found=std::find_if(sourceViews.begin(),sourceViews.end(),[&](auto const& entry){return entry.texture.get()==source.get();});
   if(found==sourceViews.end()){
    if(sourceViews.size()>=size_t(g.captureBuffers))sourceViews.erase(sourceViews.begin());
    SourceView entry;entry.texture=source;check_hresult(device->CreateShaderResourceView(source.get(),nullptr,entry.view.put()));sourceViews.push_back(std::move(entry));found=sourceViews.end()-1;
   }

   auto srv=found->view.get();auto uav=colorUav.get();ctx->CSSetShader(direct.get(),nullptr,0);ctx->CSSetShaderResources(0,1,&srv);ctx->CSSetUnorderedAccessViews(0,1,&uav,nullptr);ctx->Dispatch(45,1,1);
   ID3D11ShaderResourceView* nullSrv=nullptr;ID3D11UnorderedAccessView* nullUav=nullptr;ctx->CSSetShaderResources(0,1,&nullSrv);ctx->CSSetUnorderedAccessViews(0,1,&nullUav,nullptr);ctx->CopyResource(stage.get(),colors.get());

   D3D11_MAPPED_SUBRESOURCE mapped{};check_hresult(ctx->Map(stage.get(),0,D3D11_MAP_READ,0,&mapped));
   {std::lock_guard guard(g.lock);memcpy(g.colors.data(),mapped.pData,45*16);g.hasColors=true;g.captured++;g.sourceAt=begin;g.compositorAt=compositorAt;g.readyAt=ms();g.colorSequence++;g.processMs=g.readyAt-begin;g.maxProcessMs=std::max(g.maxProcessMs,g.processMs);if(g.benchmark&&g.processSamples.size()<8192)g.processSamples.push_back(g.processMs);}
   SetEvent(g.colorsReady);
   ctx->Unmap(stage.get(),0);f.Close();
   if(ms()>memoryAt){DXGI_QUERY_VIDEO_MEMORY_INFO m{};if(intel3)intel3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&m);{std::lock_guard guard(g.lock);g.intelMiB=m.CurrentUsage/1048576.;if(nvidia){DXGI_QUERY_VIDEO_MEMORY_INFO nv{};nvidia->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&nv);g.nvidiaMiB=nv.CurrentUsage/1048576.;}}memoryAt=ms()+1000;}
  }
 }catch(...){session.Close();pool.FrameArrived(token);pool.Close();throw;}
 session.Close();pool.FrameArrived(token);pool.Close();
}
void captureWorker(){init_apartment(apartment_type::multi_threaded);while(g.run){try{captureSession();}catch(hresult_error const&e){captureStatus(e.message().c_str());for(int i=0;i<20&&g.run;i++)Sleep(100);}catch(...){captureStatus(L"采集异常，稍后重试");Sleep(1000);}}}

std::wstring findPort(){
 HDEVINFO list=SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS,nullptr,nullptr,DIGCF_PRESENT);if(list==INVALID_HANDLE_VALUE)return {};
 std::vector<std::wstring> ports;SP_DEVINFO_DATA dev{sizeof dev};
 for(DWORD i=0;SetupDiEnumDeviceInfo(list,i,&dev);i++){
  wchar_t ids[2048]{};if(!SetupDiGetDeviceRegistryPropertyW(list,&dev,SPDRP_HARDWAREID,nullptr,reinterpret_cast<BYTE*>(ids),sizeof ids,nullptr))continue;
  std::wstring id=ids;std::transform(id.begin(),id.end(),id.begin(),towupper);if(id.find(L"VID_2E3C&PID_5740")==std::wstring::npos)continue;
  HKEY key=SetupDiOpenDevRegKey(list,&dev,DICS_FLAG_GLOBAL,0,DIREG_DEV,KEY_READ);if(key!=INVALID_HANDLE_VALUE){wchar_t name[64]{};DWORD len=sizeof name;if(RegQueryValueExW(key,L"PortName",nullptr,nullptr,reinterpret_cast<BYTE*>(name),&len)==ERROR_SUCCESS)ports.emplace_back(name);RegCloseKey(key);}}
 SetupDiDestroyDeviceInfoList(list);if(ports.size()>1)throw std::runtime_error("Multiple matching devices");return ports.empty()?L"":ports[0];
}
bool writePacket(HANDLE port,const BYTE* data,DWORD size){DWORD done=0;while(done<size){DWORD n=0;if(!WriteFile(port,data+done,size-done,&n,nullptr)||n==0)return false;done+=n;}return true;}
ledcolor::Profile ledBaseProfile;ledcolor::VisualTrim savedVisualTrim;std::shared_ptr<const ledcolor::Encoder> ledEncoder=std::make_shared<ledcolor::Encoder>();bool visualPreview=false;std::array<BYTE,3> visualBytes{};HWND visualWindow=nullptr;void openVisualWizard();bool ledProfileValid=true;std::wstring ledProfileStatus=L"LED 色彩：未标定（兼容曲线）";
void serialWorker(){
 if(!ledProfileValid){serialStatus(L"LED 色彩配置无效，已停止输出");return;}
 Handle timer(CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS));if(!timer.h){serialStatus(L"高精度定时器不可用");return;}
 while(g.run){
  try{
   auto name=findPort();if(name.empty()){serialStatus(L"未连接灯带，等待 USB…");Sleep(1000);continue;}
   Handle port(CreateFileW((std::wstring(2,wchar_t(92))+L"."+wchar_t(92)+name).c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr));
   if(port.h==INVALID_HANDLE_VALUE){serialStatus(L"串口被占用或无法打开，请退出 Ansnake");Sleep(1000);continue;}
   DCB dcb{sizeof dcb};if(!GetCommState(port.h,&dcb))throw std::runtime_error("GetCommState");dcb.BaudRate=115200;dcb.ByteSize=8;dcb.Parity=NOPARITY;dcb.StopBits=ONESTOPBIT;dcb.fBinary=TRUE;dcb.fParity=FALSE;dcb.fOutxCtsFlow=FALSE;dcb.fOutxDsrFlow=FALSE;dcb.fDtrControl=DTR_CONTROL_DISABLE;dcb.fDsrSensitivity=FALSE;dcb.fOutX=FALSE;dcb.fInX=FALSE;dcb.fRtsControl=RTS_CONTROL_DISABLE;
   if(!SetCommState(port.h,&dcb))throw std::runtime_error("SetCommState");COMMTIMEOUTS timeouts{};timeouts.ReadIntervalTimeout=MAXDWORD;timeouts.WriteTotalTimeoutConstant=150;SetCommTimeouts(port.h,&timeouts);PurgeComm(port.h,PURGE_RXCLEAR|PURGE_TXCLEAR);
   BYTE query[]={65,100,48};if(!writePacket(port.h,query,3))throw std::runtime_error("Query write");BYTE reply=0;DWORD n=0;for(int i=0;i<50&&g.run;i++){if(!ReadFile(port.h,&reply,1,&n,nullptr))throw std::runtime_error("Query read");if(n)break;Sleep(10);}if(!n||(reply&254)!=128)throw std::runtime_error("Unexpected device identity");
   serialStatus(L"已连接 P5 · "+name);double next=ms(),lastSent=0;std::array<BYTE,543> previous{};bool hadFrame=false,wasIdentifying=false;std::array<BYTE,543> beforeIdentify{};beforeIdentify[0]=65;beforeIdentify[1]=100;beforeIdentify[2]=97;uint64_t sentSequence=0;
   while(g.run){if(!g.enabled&&!g.identifying&&!g.restoreAfterIdentify){Sleep(150);next=ms();continue;}
    if(g.fixedSendClock){waitFor(timer.h,next);next+=1000./60.;if(next<ms())next=ms()+1000./60.;}
    else {WaitForSingleObject(g.colorsReady,100);if(!g.run)break;waitFor(timer.h,next);}
    BYTE rx[256];DWORD count=0;if(!ReadFile(port.h,rx,sizeof rx,&count,nullptr))throw std::runtime_error("Disconnected");if(count){std::lock_guard guard(g.lock);g.replies+=count;}
    int selectedLed=g.identifyLed.load();bool identifying=g.identifying.load(),restore=g.restoreAfterIdentify.load();if(!g.enabled&&!identifying&&!restore)continue;if(identifying&&!wasIdentifying){if(hadFrame)beforeIdentify=previous;wasIdentifying=true;}Config cfg;std::array<float,180> colors;bool valid;double sourceAt,readyAt,compositorAt;uint64_t sequence;bool visual;std::array<BYTE,3> preview;std::shared_ptr<const ledcolor::Encoder> encoder;
    {std::lock_guard guard(g.lock);cfg=g.config;visual=visualPreview;preview=visualBytes;encoder=ledEncoder;colors=g.colors;valid=g.hasColors;sourceAt=g.sourceAt;readyAt=g.readyAt;compositorAt=g.compositorAt;sequence=g.colorSequence;}
    if(cfg.mode==0&&!valid&&!identifying&&!restore)continue;std::array<BYTE,543> packet{};packet[0]=65;packet[1]=100;packet[2]=97;
    for(int i=0;i<45;i++){int source=(i+cfg.offset)%45;if(cfg.reverse)source=44-source;
     ledcolor::RGB rgb{};for(int channel=0;channel<3;channel++){float value;
      if(identifying)value=(i==selectedLed)?.15f:0;
      else if(cfg.mode==1){const float warm[]={1.f,.65f,.35f};value=warm[channel];}
      else if(cfg.mode>=2){int side=cfg.calibrated?cornerSide(source+1,cfg.corners):(source<cfg.left?0:source<cfg.left+cfg.top?1:source<cfg.left+cfg.top+cfg.right?2:3);value=(side==cfg.mode-2)?.2f:0;}
      else {int a=std::max(0,source-1),b=std::min(44,source+1);value=.25f*colors[a*4+channel]+.5f*colors[source*4+channel]+.25f*colors[b*4+channel];}
      rgb[channel]=value;}
     if(visual){for(int ch=0;ch<3;ch++)packet[3+i*3+ch]=preview[ch];}
     else if(identifying){BYTE v=i==selectedLed?108:0;for(int ch=0;ch<3;ch++)packet[3+i*3+ch]=v;}
     else {auto output=encoder->encode(rgb,cfg.brightness/100.f);for(int ch=0;ch<3;ch++)packet[3+i*3+ch]=output[ch];}}
    if(restore&&!identifying)packet=beforeIdentify;
    // Keep one-second conservative keepalive; never accumulate old frames.
    bool changedPacket=!hadFrame||packet!=previous;
    if(!changedPacket&&ms()-lastSent<1000&&!restore)continue;
    double begin=ms();if(!writePacket(port.h,packet.data(),DWORD(packet.size())))throw std::runtime_error("Write timeout");
    // Capture owns the 60 Hz cadence. Follow fresh colors instead of retaining
    // a second, unrelated clock phase. Allow 1 ms scheduling jitter, while
    // bounding adjacent USB writes and never replaying a backlog after stalls.
    if(!g.fixedSendClock)next=begin+1000./60.-1.;
    if(g.benchmark&&cfg.mode==0&&changedPacket&&sequence!=sentSequence&&g.freshAges.size()<8192){g.freshAges.push_back(begin-sourceAt);g.readyAges.push_back(begin-readyAt);g.compositorAges.push_back(begin-compositorAt);}sentSequence=sequence;
    {std::lock_guard guard(g.lock);g.sent++;g.writeMs=ms()-begin;g.maxWriteMs=std::max(g.maxWriteMs,g.writeMs);if(g.benchmark&&g.writeSamples.size()<8192){g.writeSamples.push_back(g.writeMs);if(g.lastSendAt>0)g.sendIntervals.push_back(begin-g.lastSendAt);g.internalAges.push_back(begin-sourceAt);}g.lastSendAt=begin;}
    if(restore&&!identifying){g.restoreAfterIdentify=false;wasIdentifying=false;}previous=packet;hadFrame=true;lastSent=ms();
   }
  }catch(...){serialStatus(L"连接中断或设备响应异常，正在重试…");for(int i=0;i<10&&g.run;i++)Sleep(100);}
 }
}

#include "CalibrationUI.h"
#include "VisualCalibration.h"
void save(){Config c=config();const wchar_t* cornerKeys[]={L"CornerBR",L"CornerTR",L"CornerTL",L"CornerBL"};for(int j=0;j<4;j++)WritePrivateProfileStringW(L"Settings",cornerKeys[j],std::to_wstring(c.corners[j]).c_str(),iniPath.c_str());WritePrivateProfileStringW(L"Settings",L"CornerLayout",c.calibrated?L"1":L"0",iniPath.c_str());const wchar_t* keys[]={L"Brightness",L"Depth",L"Left",L"Top",L"Right",L"Bottom",L"Reverse",L"Offset",L"Screen",L"Mode"};int values[]={c.brightness,c.depth,c.left,c.top,c.right,c.bottom,c.reverse,c.offset,c.screen,c.mode};WritePrivateProfileStringW(L"Settings",L"Filter",nullptr,iniPath.c_str());WritePrivateProfileStringW(L"Settings",L"Height",nullptr,iniPath.c_str());static_assert(std::size(keys)==std::size(values));for(size_t i=0;i<std::size(keys);i++)WritePrivateProfileStringW(L"Settings",keys[i],std::to_wstring(values[i]).c_str(),iniPath.c_str());}
HWND control(const wchar_t* klass,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id){auto c=CreateWindowExW(0,klass,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,windowHandle,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandle(nullptr),nullptr);SendMessage(c,WM_SETFONT,reinterpret_cast<WPARAM>(fontHandle),TRUE);return c;}
void updateConfig(bool rebuild){Config c=config();c.brightness=int(SendMessage(brightnessSlider,TBM_GETPOS,0,0));c.depth=int(SendMessage(depthSlider,TBM_GETPOS,0,0));c.reverse=SendMessage(reverseBox,BM_GETCHECK,0,0)==BST_CHECKED;c.mode=int(SendMessage(modeCombo,CB_GETCURSEL,0,0));c.screen=int(SendMessage(screenCombo,CB_GETCURSEL,0,0));
 {std::lock_guard guard(g.lock);g.config=c;if(rebuild)g.hasColors=false;}if(rebuild)g.revision++;save();}
void refreshStatus(){std::lock_guard guard(g.lock);wchar_t b[1024];swprintf_s(b,L"%s\n%s\n%s\n取色处理 %.2f ms · %d 个采集缓冲\n采集 %llu 帧 · 发送 %llu 帧 · 回应 %llu\n处理设备见上方 · 60 Hz 目标 · 无时间平滑\n边缘为核中心 · 角灯为四分之一圆高斯。",g.serialStatus.c_str(),g.captureStatus.c_str(),ledProfileStatus.c_str(),g.processMs,g.captureBuffers,g.captured,g.sent,g.replies);SetWindowTextW(statusLabel,b);}
void toggle(){g.enabled=!g.enabled;g.revision++;{std::lock_guard guard(g.lock);g.hasColors=false;}SetWindowTextW(enableButton,g.enabled?L"暂停同步":L"继续同步");}
LRESULT CALLBACK windowProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_CREATE:{windowHandle=h;fontHandle=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");Config c=config();
  control(L"STATIC",L"屏幕灯 · 独显版",0,24,18,200,28,0);control(L"BUTTON",L"四角校准",BS_PUSHBUTTON,225,16,110,32,18);enableButton=control(L"BUTTON",L"暂停同步",BS_PUSHBUTTON,340,16,110,32,10);
  control(L"STATIC",L"亮度",0,24,65,55,24,0);brightnessSlider=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS,90,60,350,35,11);SendMessage(brightnessSlider,TBM_SETRANGE,TRUE,MAKELONG(0,100));SendMessage(brightnessSlider,TBM_SETPOS,TRUE,c.brightness);
  control(L"STATIC",L"模式",0,24,110,55,24,0);modeCombo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,90,105,350,200,12);for(auto s:{L"跟随屏幕",L"静态暖色",L"校准：左边",L"校准：上边",L"校准：右边",L"校准：下边"})SendMessageW(modeCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s));SendMessage(modeCombo,CB_SETCURSEL,c.mode,0);
  control(L"STATIC",L"屏幕",0,24,155,55,24,0);screenCombo=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,90,150,350,160,13);for(size_t i=0;i<monitors.size();i++){MONITORINFOEXW m{};m.cbSize=sizeof m;GetMonitorInfoW(monitors[i],&m);std::wstring s=std::to_wstring(i+1)+L" · "+m.szDevice;SendMessageW(screenCombo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(s.c_str()));}SendMessage(screenCombo,CB_SETCURSEL,c.screen,0);
  control(L"STATIC",L"全像素边缘高斯 · 原始分辨率取色",0,24,200,420,28,0);
  control(L"STATIC",L"深度 / 高度%",0,24,245,110,24,0);depthSlider=control(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS,140,240,235,35,15);depthValue=control(L"STATIC",(std::to_wstring(c.depth)+L"%").c_str(),0,390,245,60,24,0);SendMessage(depthSlider,TBM_SETRANGE,TRUE,MAKELONG(2,30));SendMessage(depthSlider,TBM_SETPOS,TRUE,c.depth);
  control(L"STATIC",L"每边灯数：左 / 上 / 右 / 下（合计 45）",0,24,290,410,24,0);int counts[]={c.left,c.top,c.right,c.bottom};for(int i=0;i<4;i++)sideEdits[i]=control(L"EDIT",std::to_wstring(counts[i]).c_str(),WS_BORDER|ES_NUMBER,24+i*76,320,60,28,20+i);
  reverseBox=control(L"BUTTON",L"反转方向",BS_AUTOCHECKBOX,24,365,120,28,25);SendMessage(reverseBox,BM_SETCHECK,c.reverse?BST_CHECKED:BST_UNCHECKED,0);control(L"STATIC",L"起点偏移",0,170,368,90,24,0);offsetEdit=control(L"EDIT",std::to_wstring(c.offset).c_str(),WS_BORDER|ES_NUMBER,265,365,55,28,26);control(L"BUTTON",L"改用均分",BS_PUSHBUTTON,340,320,100,72,27);EnableWindow(reverseBox,!c.calibrated);EnableWindow(offsetEdit,!c.calibrated);
  statusLabel=control(L"STATIC",L"",0,24,410,430,190,0);SetTimer(h,1,1000,nullptr);
  tray.cbSize=sizeof tray;tray.hWnd=h;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=WM_APP+1;tray.hIcon=LoadIcon(nullptr,IDI_APPLICATION);wcscpy_s(tray.szTip,L"屏幕灯 · 独显");Shell_NotifyIconW(NIM_ADD,&tray);return 0;}
 case WM_TIMER:refreshStatus();return 0;
 case WM_DISPLAYCHANGE:g.revision++;return 0;
 case WM_HSCROLL:if(reinterpret_cast<HWND>(lp)==depthSlider)SetWindowTextW(depthValue,(std::to_wstring(int(SendMessage(depthSlider,TBM_GETPOS,0,0)))+L"%").c_str());if(LOWORD(wp)==TB_ENDTRACK)updateConfig(reinterpret_cast<HWND>(lp)==depthSlider);return 0;
 case WM_COMMAND:{int id=LOWORD(wp);if(id==10)toggle();else if(id==18)openCalibration();else if((id==12||id==13)&&HIWORD(wp)==CBN_SELCHANGE)updateConfig(true);
else if(id==25)updateConfig(false);else if(id==27){Config c=config();int* counts[]={&c.left,&c.top,&c.right,&c.bottom};int sum=0;for(int i=0;i<4;i++){*counts[i]=GetDlgItemInt(h,20+i,nullptr,FALSE);sum+=*counts[i];}c.offset=GetDlgItemInt(h,26,nullptr,FALSE);c.calibrated=false;if(sum!=45||c.left<1||c.top<1||c.right<1||c.offset>44){MessageBoxW(h,L"左右和上边至少各 1 灯，合计必须为 45。起点偏移为 0–44。",L"检查布局",MB_OK);return 0;}{std::lock_guard guard(g.lock);g.config=c;g.hasColors=false;}g.revision++;EnableWindow(reverseBox,TRUE);EnableWindow(offsetEdit,TRUE);save();}else if(id==100){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}else if(id==101)toggle();else if(id==102)DestroyWindow(h);return 0;}
 case WM_APP+2:openCalibration();return 0;
 case WM_APP+1:if(lp==WM_LBUTTONUP||lp==WM_LBUTTONDBLCLK){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}else if(lp==WM_RBUTTONUP){HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,100,L"打开设置");AppendMenuW(m,MF_STRING,101,g.enabled?L"暂停":L"继续");AppendMenuW(m,MF_STRING,102,L"退出");POINT p;GetCursorPos(&p);SetForegroundWindow(h);TrackPopupMenu(m,TPM_RIGHTBUTTON,p.x,p.y,0,h,nullptr);DestroyMenu(m);}return 0;
 case WM_CLOSE:ShowWindow(h,SW_HIDE);return 0;
 case WM_DESTROY:if(IsWindow(calibrationWindow))DestroyWindow(calibrationWindow);KillTimer(h,1);Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}

#include "Startup.h"
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR cmd,int show){
 int startupResult=startupCommand(cmd);if(startupResult>=0)return startupResult;bool startInTray=wcsstr(cmd,L"--tray")!=nullptr;
 bool calibrationOnly=wcsstr(cmd,L"--calibrate")!=nullptr;
#ifdef CALIBRATION_ONLY
 calibrationOnly=wcsstr(cmd,L"--benchmark")==nullptr;
#endif
 Handle singleton(CreateMutexW(nullptr,FALSE,L"Local\\AnsnakeScreenGlow"));if(GetLastError()==ERROR_ALREADY_EXISTS){if(startInTray)return 0;if(calibrationOnly){if(auto main=FindWindowW(L"ScreenGlowWindow",nullptr)){SendMessageW(main,WM_APP+2,0,0);return 0;}if(auto other=FindWindowW(L"ScreenGlowCalibration",nullptr)){ShowWindow(other,SW_RESTORE);SetForegroundWindow(other);return 0;}}MessageBoxW(nullptr,L"屏幕灯已经在运行，请从托盘打开。",L"屏幕灯",MB_OK);return 0;}
 SetProcessDPIAware();EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM)->BOOL{monitors.push_back(m);return TRUE;},0);if(monitors.empty())return 1;
 wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);iniPath=std::wstring(path);iniPath=iniPath.substr(0,iniPath.find_last_of(L"\\")+1)+L"ScreenGlow.ini";
 try {auto profilePath=std::filesystem::path(iniPath).parent_path()/L"LedColor.profile";if(std::filesystem::exists(profilePath)){std::ifstream input(profilePath);if(!input)throw std::runtime_error("Cannot read profile");auto profile=ledcolor::read(input);ledBaseProfile=profile;ledEncoder=std::make_shared<ledcolor::Encoder>(profile);ledProfileStatus=L"LED 色彩：设备配置（色准待验证）";}}catch(...){ledProfileValid=false;ledProfileStatus=L"LED 色彩：配置无效，停止输出";}
 try {auto path=std::filesystem::path(iniPath).parent_path()/L"LedVisual.profile";if(std::filesystem::exists(path)){std::ifstream input(path);savedVisualTrim=ledcolor::readTrim(input);ledEncoder=std::make_shared<ledcolor::Encoder>(ledBaseProfile,savedVisualTrim);if(ledProfileValid)ledProfileStatus=L"LED 色彩：肉眼校准（非仪器标定）";}}catch(...){ledProfileValid=false;ledProfileStatus=L"肉眼校准配置无效，停止输出";}
 Config c;auto read=[&](const wchar_t*k,int fallback,int lo,int hi){return std::clamp(int(GetPrivateProfileIntW(L"Settings",k,fallback,iniPath.c_str())),lo,hi);};c.brightness=read(L"Brightness",40,0,100);c.depth=read(L"Depth",12,2,30);c.left=read(L"Left",12,1,43);c.top=read(L"Top",21,1,43);c.right=read(L"Right",12,1,43);c.bottom=read(L"Bottom",0,0,42);if(c.left+c.top+c.right+c.bottom!=45){c.left=12;c.top=21;c.right=12;c.bottom=0;}c.reverse=read(L"Reverse",0,0,1);c.offset=read(L"Offset",0,0,44);c.screen=read(L"Screen",0,0,int(monitors.size())-1);c.mode=read(L"Mode",0,0,1);g.config=c;
 c.corners={read(L"CornerBR",0,0,45),read(L"CornerTR",0,0,45),read(L"CornerTL",0,0,45),read(L"CornerBL",0,0,45)};c.calibrated=read(L"CornerLayout",0,0,1)&&validCorners(c.corners);if(c.calibrated){c.reverse=0;c.offset=0;}g.config=c;

 bool benchmark=wcsstr(cmd,L"--benchmark")!=nullptr;bool dry=wcsstr(cmd,L"--dry-run")!=nullptr;
  auto argument=[&](const wchar_t* key,int fallback){auto p=wcsstr(cmd,key);return p?_wtoi(p+wcslen(key)):fallback;};
 int seconds=std::clamp(argument(L"--seconds=",15),5,120);
 
 g.captureBuffers=std::clamp(argument(L"--capture-buffers=",2),1,3);g.benchmark=benchmark;g.captureOnly=wcsstr(cmd,L"--capture-only")!=nullptr;
 if(benchmark){g.processSamples.reserve(8192);g.writeSamples.reserve(8192);g.sendIntervals.reserve(8192);g.internalAges.reserve(8192);}
 Handle colorsReady(CreateEventW(nullptr,FALSE,FALSE,nullptr));if(!colorsReady.h)return 3;g.colorsReady=colorsReady.h;
 
 g.fixedSendClock=wcsstr(cmd,L"--fixed-send-clock")!=nullptr;

 if(benchmark){g.transferSamples.reserve(8192);g.compositorAges.reserve(8192);}
 if(benchmark){g.freshAges.reserve(8192);g.readyAges.reserve(8192);}
 if(calibrationOnly)g.identifying=true;if(dry)serialStatus(L"离线预览：不控制灯带");
 std::thread capture(captureWorker);std::thread serial;if(!dry)serial=std::thread(serialWorker);
 if(calibrationOnly){INITCOMMONCONTROLSEX init{sizeof init,ICC_STANDARD_CLASSES};InitCommonControlsEx(&init);openCalibration(true);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(!visualWindow&&msg.message==WM_KEYDOWN&&msg.hwnd!=calibrationNumber&&(msg.wParam==VK_LEFT||msg.wParam==VK_RIGHT)){selectCalibration(g.identifyLed.load()+(msg.wParam==VK_RIGHT?2:0));continue;}if(visualWindow&&msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(visualWindow);continue;}if(visualWindow&&IsDialogMessageW(visualWindow,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}if(serial.joinable())for(int i=0;i<35&&g.restoreAfterIdentify;i++)Sleep(10);g.run=false;capture.join();if(serial.joinable())serial.join();return 0;}
 if(benchmark){Sleep(seconds*1000);g.run=false;capture.join();if(serial.joinable())serial.join();std::wstring report=iniPath.substr(0,iniPath.find_last_of(L"\\")+1)+L"benchmark.txt";std::ofstream file(report);std::lock_guard guard(g.lock);file<<"capture="<<winrt::to_string(g.captureStatus)<<"\nserial="<<winrt::to_string(g.serialStatus)<<"\nframes="<<g.captured<<"\nsent="<<g.sent<<"\nreplies="<<g.replies<<"\nlast_process_ms="<<g.processMs<<"\nmax_process_ms="<<g.maxProcessMs<<"\nmax_write_ms="<<g.maxWriteMs<<"\nintel_graphics_MiB="<<g.intelMiB<<"\nnvidia_process_local_MiB="<<g.nvidiaMiB<<"\n"; if(!g.writeSamples.empty()){file<<"first_write_ms="<<g.writeSamples.front()<<"\nmax_write_index="<<std::distance(g.writeSamples.begin(),std::max_element(g.writeSamples.begin(),g.writeSamples.end()))<<"\nwrite_over_50ms="<<std::count_if(g.writeSamples.begin(),g.writeSamples.end(),[](double t){return t>50;})<<"\n";} file<<"capture_buffers="<<g.captureBuffers<<"\n"; auto stats=[&](const char*name,std::vector<double>&v){if(v.empty())return;std::sort(v.begin(),v.end());file<<name<<"_n="<<v.size()<<"\n"<<name<<"_median="<<v[v.size()/2]<<"\n"<<name<<"_p95="<<v[std::min(v.size()-1,size_t(v.size()*.95))]<<"\n"<<name<<"_p99="<<v[std::min(v.size()-1,size_t(v.size()*.99))]<<"\n"<<name<<"_max="<<v.back()<<"\n";};
 if(!g.processSamples.empty())file<<"first_process_ms="<<g.processSamples.front()<<"\nmax_process_index="<<std::distance(g.processSamples.begin(),std::max_element(g.processSamples.begin(),g.processSamples.end()))<<"\nprocess_over_50ms="<<std::count_if(g.processSamples.begin(),g.processSamples.end(),[](double t){return t>50;})<<"\n";
 stats("process_ms",g.processSamples);stats("transfer_cpu_ms",g.transferSamples);stats("raw_frame_timestamp_delta_ms",g.compositorAges);stats("write_ms",g.writeSamples);stats("send_interval_ms",g.sendIntervals);stats("internal_age_ms",g.internalAges);stats("fresh_internal_age_ms",g.freshAges);stats("ready_to_send_ms",g.readyAges);file<<"raw_hardware="<<g.rawHardware<<"\nfilter=edge-gaussian"<<"\ngpu_path="<<g.gpuPath<<"\nfixed_send_clock="<<g.fixedSendClock<<"\nduration_seconds="<<seconds<<"\n";return g.captured>0?0:2;}
 INITCOMMONCONTROLSEX ic{sizeof ic,ICC_BAR_CLASSES};InitCommonControlsEx(&ic);WNDCLASSW wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"ScreenGlowWindow";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);RegisterClassW(&wc);HWND h=CreateWindowW(wc.lpszClassName,L"屏幕灯 · 独显版",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,490,700,nullptr,nullptr,instance,nullptr);
 if(h){if(!startInTray)ShowWindow(h,show);MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){if(calibrationWindow&&!visualWindow&&msg.message==WM_KEYDOWN&&msg.hwnd!=calibrationNumber&&(msg.wParam==VK_LEFT||msg.wParam==VK_RIGHT)){selectCalibration(g.identifyLed.load()+(msg.wParam==VK_RIGHT?2:0));continue;}if(visualWindow&&msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(visualWindow);continue;}if(visualWindow&&IsDialogMessageW(visualWindow,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);}}if(serial.joinable())for(int i=0;i<35&&g.restoreAfterIdentify;i++)Sleep(10);g.run=false;capture.join();if(serial.joinable())serial.join();if(fontHandle)DeleteObject(fontHandle);return h?0:1;
}

















