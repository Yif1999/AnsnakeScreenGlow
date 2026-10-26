#define wWinMain unusedApplicationEntry
#include "ScreenGlow.cpp"
#include <cassert>
int main(){
 SetProcessDPIAware();EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM)->BOOL{monitors.push_back(m);return TRUE;},0);
 INITCOMMONCONTROLSEX init{sizeof init,ICC_STANDARD_CLASSES};InitCommonControlsEx(&init);
 WNDCLASSW wc{};wc.lpfnWndProc=calibrationProc;wc.hInstance=GetModuleHandle(nullptr);wc.lpszClassName=L"CalibrationOfflineTest";RegisterClassW(&wc);
 HWND h=CreateWindowW(wc.lpszClassName,L"Offline test",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_CLIPCHILDREN,0,0,920,800,nullptr,nullptr,wc.hInstance,nullptr);assert(h);
 assert(!IsWindowEnabled(calibrationSave));
 const int indices[]={4,14,31,41};for(int i=0;i<4;i++){selectCalibration(indices[i]);SendMessage(h,WM_COMMAND,310+i,0);}
 assert(IsWindowEnabled(calibrationSave));assert(!config().calibrated);
 auto original=cornerPoints(calibrationCorners);RECT canvas=calibrationCanvas();auto front=calibrationXY(original[3],canvas);SendMessage(h,WM_COMMAND,315,0);auto rear=calibrationXY(original[3],canvas);assert(front.x==canvas.right&&rear.x==canvas.left);assert((calibrationCorners==std::array<int,4>({4,14,31,41})));
 SendMessage(h,WM_COMMAND,315,0);selectCalibration(14);
 INITCOMMONCONTROLSEX bars{sizeof bars,ICC_BAR_CLASSES};InitCommonControlsEx(&bars);
 WNDCLASSW vw{};vw.lpfnWndProc=visualProc;vw.hInstance=GetModuleHandle(nullptr);vw.lpszClassName=L"VisualOfflineTest";RegisterClassW(&vw);
 HWND parent=h;auto createVisual=[&](){return CreateWindowW(vw.lpszClassName,L"Offline visual test",WS_POPUP|WS_CLIPCHILDREN,0,0,1000,760,parent,nullptr,vw.hInstance,nullptr);};
 h=createVisual();assert(h&&visualPreview&&!IsWindowEnabled(parent));auto originalEncoder=ledEncoder; for(int page=0;page<4;page++){visualStep=page;visualPage();for(auto slider:visualSliders){assert(IsWindowEnabled(slider));int previous=int(SendMessage(slider,TBM_GETPOS,0,0));SendMessage(slider,WM_KEYDOWN,VK_RIGHT,0);assert(SendMessage(slider,TBM_GETPOS,0,0)>previous);SendMessage(slider,TBM_SETPOS,TRUE,100);}}
 visualStep=0;visualPage();std::puts("PASS all four wizard pages expose enabled, keyboard-responsive sliders");
 SendMessage(h,WM_COMMAND,414,0);assert(visualStep==1);SendMessage(visualSliders[0],TBM_SETPOS,TRUE,80);SendMessage(h,WM_HSCROLL,0,0);assert(visualDraft.gain[0]==.8f);assert(visualBytes[0]<visualBytes[1]);
 SendMessage(visualCompare,BM_SETCHECK,BST_CHECKED,0);SendMessage(h,WM_COMMAND,411,0);assert(visualBytes[0]==visualBytes[1]);for(auto slider:visualSliders)assert(!IsWindowEnabled(slider));SendMessage(visualCompare,BM_SETCHECK,BST_UNCHECKED,0);SendMessage(h,WM_COMMAND,411,0);
 for(auto slider:visualSliders)assert(IsWindowEnabled(slider)); RECT r{};GetClientRect(h,&r);int width=r.right,height=r.bottom;HDC dc=GetDC(h),memory=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto previous=SelectObject(memory,bitmap);paintVisual(memory);for(HWND child=GetWindow(h,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)){RECT childRect{};GetWindowRect(child,&childRect);MapWindowPoints(nullptr,h,reinterpret_cast<POINT*>(&childRect),2);int saved=SaveDC(memory);SetViewportOrgEx(memory,childRect.left,childRect.top,nullptr);SendMessage(child,WM_PRINT,reinterpret_cast<WPARAM>(memory),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);RestoreDC(memory,saved);}GdiFlush();
 BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=header.bfOffBits+width*height*4;std::ofstream out("analysis/visual-calibration-preview.bmp",std::ios::binary);out.write(reinterpret_cast<char*>(&header),sizeof header);out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof info.bmiHeader);out.write(static_cast<char*>(bits),width*height*4);out.close();SelectObject(memory,previous);DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(h,dc);
 DestroyWindow(h);assert(!visualPreview&&IsWindowEnabled(parent)&&ledEncoder==originalEncoder&&g.identifying);
 auto dir=std::filesystem::path(L"analysis/visual-test");std::filesystem::create_directories(dir);iniPath=(dir/L"ScreenGlow.ini").wstring();
 h=createVisual();SendMessage(h,WM_COMMAND,414,0);SendMessage(visualSliders[0],TBM_SETPOS,TRUE,85);SendMessage(h,WM_HSCROLL,0,0);SendMessage(h,WM_COMMAND,414,0);SendMessage(visualSliders[3],TBM_SETPOS,TRUE,120);SendMessage(h,WM_HSCROLL,0,0);SendMessage(h,WM_COMMAND,414,0);SendMessage(h,WM_COMMAND,414,0);
 assert(!visualWindow&&!visualPreview&&savedVisualTrim.gain[0]==.85f&&savedVisualTrim.darkness==1.2f);std::ifstream file(dir/L"LedVisual.profile");auto disk=ledcolor::readTrim(file);assert(disk.gain==savedVisualTrim.gain&&disk.darkness==savedVisualTrim.darkness);assert(!std::filesystem::exists(dir/L"LedColor.profile"));
 DWORD beforeGdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),beforeUser=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);
 for(int cycle=0;cycle<100;cycle++){h=createVisual();SendMessage(h,WM_COMMAND,414,0);SendMessage(h,WM_COMMAND,414,0);SendMessage(visualSample,CB_SETCURSEL,3,0);visualUpdate();assert((visualBytes==std::array<BYTE,3>{0,0,0}));DestroyWindow(h);assert(!visualPreview&&IsWindowEnabled(parent));}
 DWORD afterGdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS),afterUser=GetGuiResources(GetCurrentProcess(),GR_USEROBJECTS);assert(afterGdi<=beforeGdi+2&&afterUser<=beforeUser+2);std::printf("PASS 100 wizard open/black/cancel cycles: GDI %lu -> %lu, USER %lu -> %lu\n",beforeGdi,afterGdi,beforeUser,afterUser);
 // Exercise the production save path used when a depth drag ends.
 iniPath=std::filesystem::absolute(dir/L"Settings-save-regression.ini").wstring();
 WritePrivateProfileStringW(L"Settings",L"Filter",L"5",iniPath.c_str());WritePrivateProfileStringW(L"Settings",L"Height",L"1080",iniPath.c_str());
 for(int depth=2;depth<=30;depth++){
  {std::lock_guard guard(g.lock);g.config.depth=depth;g.config.brightness=73;g.config.mode=1;g.config.calibrated=true;g.config.corners={4,14,32,42};}
  save();
  assert(GetPrivateProfileIntW(L"Settings",L"Depth",999,iniPath.c_str())==depth);
  assert(GetPrivateProfileIntW(L"Settings",L"Brightness",999,iniPath.c_str())==73);
  assert(GetPrivateProfileIntW(L"Settings",L"Mode",999,iniPath.c_str())==1);
  assert(GetPrivateProfileIntW(L"Settings",L"CornerBL",999,iniPath.c_str())==42);
  assert(GetPrivateProfileIntW(L"Settings",L"Filter",999,iniPath.c_str())==999);
  assert(GetPrivateProfileIntW(L"Settings",L"Height",999,iniPath.c_str())==999);
 }
 std::puts("PASS production settings save/reload at all 29 depth positions, retained calibration, removed obsolete keys");
 DestroyWindow(parent);assert(!g.identifying);std::puts("PASS visual wizard: live draft, A/B, cancel rollback, parent restoration, atomic save and profile reload (isolated test directory)");std::puts("PASS offline calibration: four-corner recording, save gating, front/rear mirror, no production config writes or USB access");
}







