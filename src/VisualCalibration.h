#pragma once
// A separate, reversible visual trim; never overwrites the measured device profile.
ledcolor::VisualTrim visualDraft;int visualStep=0;bool visualBefore=false,visualWasIdentifying=false;
HWND visualTitle,visualHint,visualSample,visualSliders[4],visualValues[4],visualBack,visualNext,visualCompare,visualInfo;HFONT visualFont;
int visualPanelX=0,visualPanelY=0;ledcolor::RGB visualTarget{.5f,.5f,.5f};
HWND visualControl(const wchar_t*type,const wchar_t*text,DWORD style,int x,int y,int w,int height,int id){auto h=CreateWindowW(type,text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,visualPanelX+x,visualPanelY+y,w,height,visualWindow,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandle(nullptr),nullptr);SendMessage(h,WM_SETFONT,reinterpret_cast<WPARAM>(visualFont),TRUE);return h;}
float visualLinear(float x){return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
void visualUpdate(){
 int sample=std::max(0,int(SendMessage(visualSample,CB_GETCURSEL,0,0)));
 if(visualStep<=1){float level=sample==0?.75f:sample==1?.5f:1.f;visualTarget={level,level,level};}
 else if(visualStep==2){float level=sample==0?.15f:sample==1?.3f:sample==2?.5f:0.f;visualTarget={level,level,level};}
 else {const ledcolor::RGB colors[]={{.85f,.2f,.2f},{.2f,.85f,.2f},{.2f,.2f,.85f},{.85f,.7f,.2f},{.2f,.75f,.85f},{.85f,.3f,.7f},{.75f,.75f,.75f}};visualTarget=colors[std::min(sample,6)];}
 for(int i=0;i<4;i++){int value=int(SendMessage(visualSliders[i],TBM_GETPOS,0,0));if(i<3)visualDraft.gain[i]=value/100.f;else visualDraft.darkness=value/100.f;std::wstring label=std::to_wstring(value)+L"%";SetWindowTextW(visualValues[i],label.c_str());}
 auto trim=visualBefore?savedVisualTrim:visualDraft;ledcolor::Encoder encoder(ledBaseProfile,trim);auto linear=visualTarget;for(auto&v:linear)v=visualLinear(v);auto bytes=encoder.encode(linear,config().brightness/100.f);
 {std::lock_guard guard(g.lock);visualBytes=bytes;visualPreview=true;SetWindowTextW(visualInfo,(L"当前灯光亮度 "+std::to_wstring(g.config.brightness)+L"% · "+(visualBefore?L"对照中：取消勾选后可拖动滑条":L"预览：当前调整")+L"\r\n"+g.serialStatus).c_str());}
 if(g.colorsReady)SetEvent(g.colorsReady);InvalidateRect(visualWindow,nullptr,FALSE);
}
void visualPage(){
 const wchar_t*titles[]={L"1 / 4  准备：按日常环境观察",L"2 / 4  灰白平衡：消除明显偏色",L"3 / 4  暗部：让亮度变化自然",L"4 / 4  彩色复核：确认后保存"};
 const wchar_t*hints[]={L"保持常用环境光和屏幕亮度，观察屏幕后墙面的泛光。\r\n无需与屏幕一样亮；不要直视灯珠。先适应约半分钟。\r\n滑条可直接拖动，建议按后续步骤调整；这是肉眼观感校准。",
 L"切换浅灰、中灰、白色，观察墙面是否明显偏红、绿或蓝。\r\n偏哪种颜色就优先稍微降低对应通道，避免全部调高。\r\n三种灰度都检查；保持原来的环境光与观看位置。",
 L"依次查看暗灰、中暗灰、中灰和黑色，观察泛光变化。\r\n暗部太亮：向右调；暗部丢失太多：向左调。\r\n黑色应熄灭。不要求墙面与屏幕同亮，也不逐通道改曲线。",
 L"依次检查红、绿、蓝、黄、青、紫和灰色，避免明显偏色。\r\n若不自然，返回上一步；用“对照原设置”比较。\r\n保存的是肉眼校准配置。完成后再用日常视频复核。"};
 SetWindowTextW(visualTitle,titles[visualStep]);SetWindowTextW(visualHint,hints[visualStep]);SendMessage(visualSample,CB_RESETCONTENT,0,0);
 if(visualStep<=1){for(auto label:{L"浅灰",L"中灰",L"白色"})SendMessage(visualSample,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));}
 else if(visualStep==2){for(auto label:{L"暗灰",L"中暗灰",L"中灰",L"黑色"})SendMessage(visualSample,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));}
 else {for(auto label:{L"红",L"绿",L"蓝",L"黄",L"青",L"紫",L"灰"})SendMessage(visualSample,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));}
 SendMessage(visualSample,CB_SETCURSEL,0,0);for(int i=0;i<4;i++)EnableWindow(visualSliders[i],!visualBefore);
 EnableWindow(visualBack,visualStep>0);SetWindowTextW(visualNext,visualStep==3?L"保存并完成":L"下一步");visualUpdate();
}
bool saveVisualTrim(const ledcolor::VisualTrim&t){
 auto path=std::filesystem::path(iniPath).parent_path()/L"LedVisual.profile";auto temp=path;temp+=L".tmp";
 try{std::ofstream out(temp,std::ios::trunc);if(!out)throw std::runtime_error("open");ledcolor::writeTrim(out,t);out.flush();if(!out)throw std::runtime_error("write");out.close();if(out.fail())throw std::runtime_error("close");if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("replace");return true;}catch(...){DeleteFileW(temp.c_str());return false;}
}
void paintVisual(HDC dc){RECT r{};GetClientRect(visualWindow,&r);auto color=visualTarget;HBRUSH bg=CreateSolidBrush(RGB(int(color[0]*255+.5f),int(color[1]*255+.5f),int(color[2]*255+.5f)));FillRect(dc,&r,bg);DeleteObject(bg);RECT panel{visualPanelX,visualPanelY,visualPanelX+720,visualPanelY+620};FillRect(dc,&panel,GetSysColorBrush(COLOR_BTNFACE));}
LRESULT CALLBACK visualProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_CREATE:{visualWindow=h;RECT r{};GetClientRect(h,&r);visualPanelX=std::max(0,int(r.right-720)/2);visualPanelY=std::max(0,int(r.bottom-620)/2);visualStep=0;visualBefore=false;visualDraft=savedVisualTrim;visualWasIdentifying=g.identifying;identification(true);EnableWindow(calibrationWindow,FALSE);
  visualFont=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
  visualTitle=visualControl(L"STATIC",L"",0,24,22,670,30,0);visualHint=visualControl(L"STATIC",L"",0,24,65,672,100,0);
  visualControl(L"STATIC",L"测试画面",0,24,180,100,25,0);visualSample=visualControl(L"COMBOBOX",L"",CBS_DROPDOWNLIST,140,175,230,220,410);
  const wchar_t*labels[]={L"红通道",L"绿通道",L"蓝通道",L"暗部压低"};
  for(int i=0;i<4;i++){visualControl(L"STATIC",labels[i],0,24,225+i*48,105,28,0);visualSliders[i]=visualControl(TRACKBAR_CLASSW,L"",TBS_AUTOTICKS,140,220+i*48,430,36,420+i);SendMessage(visualSliders[i],TBM_SETRANGE,TRUE,MAKELONG(50,i<3?150:200));SendMessage(visualSliders[i],TBM_SETPOS,TRUE,int((i<3?visualDraft.gain[i]:visualDraft.darkness)*100+.5f));visualValues[i]=visualControl(L"STATIC",L"",0,590,225+i*48,95,28,0);}
  visualInfo=visualControl(L"STATIC",L"",0,24,425,670,55,0);visualCompare=visualControl(L"BUTTON",L"对照原设置",BS_AUTOCHECKBOX,24,490,180,32,411);visualControl(L"BUTTON",L"恢复进入时参数",BS_PUSHBUTTON,230,490,190,32,412);
  visualBack=visualControl(L"BUTTON",L"上一步",BS_PUSHBUTTON,24,555,130,38,413);visualNext=visualControl(L"BUTTON",L"下一步",BS_PUSHBUTTON,430,555,180,38,414);visualControl(L"BUTTON",L"取消",BS_PUSHBUTTON,625,555,75,38,415);SetTimer(h,1,1000,nullptr);visualPage();return 0;}
 case WM_TIMER:visualUpdate();return 0;
 case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);paintVisual(dc);EndPaint(h,&ps);return 0;}
 case WM_PRINTCLIENT:paintVisual(reinterpret_cast<HDC>(wp));return 0;
 case WM_ERASEBKGND:return 1;
 case WM_HSCROLL:visualUpdate();return 0;
 case WM_COMMAND:{int id=LOWORD(wp);if(id==410&&HIWORD(wp)==CBN_SELCHANGE)visualUpdate();else if(id==411){visualBefore=SendMessage(visualCompare,BM_GETCHECK,0,0)==BST_CHECKED;for(int i=0;i<4;i++)EnableWindow(visualSliders[i],!visualBefore);visualUpdate();}
  else if(id==412){for(int i=0;i<4;i++)SendMessage(visualSliders[i],TBM_SETPOS,TRUE,int((i<3?savedVisualTrim.gain[i]:savedVisualTrim.darkness)*100+.5f));visualUpdate();}
  else if(id==413&&visualStep>0){visualStep--;visualPage();}
  else if(id==414){if(visualStep<3){visualStep++;visualPage();}else {if(visualBefore){MessageBoxW(h,L"请取消“对照原设置”，检查当前调整后再保存。",L"检查预览",MB_OK);return 0;}auto encoder=std::make_shared<ledcolor::Encoder>(ledBaseProfile,visualDraft);if(!saveVisualTrim(visualDraft)){MessageBoxW(h,L"配置保存失败，原配置未改变。请检查程序目录是否可写。",L"保存失败",MB_OK);return 0;}{std::lock_guard guard(g.lock);savedVisualTrim=visualDraft;ledEncoder=encoder;ledProfileStatus=L"LED 色彩：肉眼校准（非仪器标定）";}DestroyWindow(h);}}
  else if(id==415)DestroyWindow(h);return 0;}
 case WM_KEYDOWN:if(wp==VK_ESCAPE)DestroyWindow(h);return 0;
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:KillTimer(h,1);{std::lock_guard guard(g.lock);visualPreview=false;}identification(visualWasIdentifying);visualWindow=nullptr;EnableWindow(calibrationWindow,TRUE);if(IsWindow(calibrationWindow))SetForegroundWindow(calibrationWindow);DeleteObject(visualFont);visualFont=nullptr;return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
void openVisualWizard(){if(visualWindow){SetForegroundWindow(visualWindow);return;}if(!ledProfileValid){MessageBoxW(calibrationWindow,L"设备色彩配置无效，请先修复配置再进行颜色校准。",L"无法预览",MB_OK);return;}
 INITCOMMONCONTROLSEX init{sizeof init,ICC_BAR_CLASSES};InitCommonControlsEx(&init);WNDCLASSW wc{};wc.lpfnWndProc=visualProc;wc.hInstance=GetModuleHandle(nullptr);wc.lpszClassName=L"ScreenGlowVisualCalibration";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClassW(&wc);MONITORINFO mi{sizeof mi};auto cfg=config();GetMonitorInfo(monitors[std::min<size_t>(cfg.screen,monitors.size()-1)],&mi);auto r=mi.rcMonitor;
 HWND h=CreateWindowW(wc.lpszClassName,L"颜色校准 · 肉眼调整",WS_POPUP|WS_CLIPCHILDREN,r.left,r.top,r.right-r.left,r.bottom-r.top,calibrationWindow,nullptr,wc.hInstance,nullptr);if(h){ShowWindow(h,SW_SHOW);SetForegroundWindow(h);}
}


