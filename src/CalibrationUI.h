#pragma once
// Included after State/config/serial helpers in ScreenGlow.cpp.
void save();
HWND calibrationWindow=nullptr,calibrationNumber=nullptr,calibrationStatus=nullptr,calibrationSave=nullptr,calibrationIdentify=nullptr,calibrationRecord[4]{};
HFONT calibrationFont=nullptr;std::array<int,4> calibrationCorners{};bool calibrationRear=false,calibrationOwnLoop=false;
const wchar_t* frontCornerNames[]={L"右下角",L"右上角",L"左上角",L"左下角"};
const wchar_t* rearCornerNames[]={L"左下角",L"左上角",L"右上角",L"右下角"};
void identification(bool enabled){bool was=g.identifying.exchange(enabled);if(was&&!enabled)g.restoreAfterIdentify=true;g.revision++;if(g.colorsReady)SetEvent(g.colorsReady);}
void refreshCalibration(){if(!calibrationWindow)return;int selected=g.identifyLed.load()+1;if(GetFocus()!=calibrationNumber)SetWindowTextW(calibrationNumber,std::to_wstring(selected).c_str());
 for(int i=0;i<4;i++){std::wstring text=L"记录"+std::wstring(frontCornerNames[i])+L"："+(calibrationCorners[i]?std::to_wstring(calibrationCorners[i]):L"未定位");SetWindowTextW(calibrationRecord[i],text.c_str());}
 bool valid=validCorners(calibrationCorners);EnableWindow(calibrationSave,valid);SetWindowTextW(calibrationIdentify,g.identifying?L"暂停定位灯":L"继续单灯定位");
 bool complete=std::all_of(calibrationCorners.begin(),calibrationCorners.end(),[](int n){return n>0;});std::wstring text=valid?L"四角有效。浅框为采样区，蓝色为当前灯；边灯矩形，角灯为四分之一圆。":complete?L"编号需按走线递增，且底边首尾不能重叠。请检查四角是否记录正确。":L"用上一颗 / 下一颗找到角落灯，依次记录四角；尚未改动已保存布局。";
 {std::lock_guard guard(g.lock);text+=L"\r\n"+g.serialStatus;}
 SetWindowTextW(calibrationStatus,text.c_str());InvalidateRect(calibrationWindow,nullptr,FALSE);
}
void selectCalibration(int n){g.identifyLed=std::clamp(n,1,45)-1;if(g.colorsReady)SetEvent(g.colorsReady);refreshCalibration();}
HWND calibrationControl(const wchar_t*type,const wchar_t*text,DWORD style,int x,int y,int w,int h,int id){auto control=CreateWindowExW(0,type,text,WS_CHILD|WS_VISIBLE|style|(wcscmp(type,L"STATIC")?WS_TABSTOP:0),x,y,w,h,calibrationWindow,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandle(nullptr),nullptr);SendMessageW(control,WM_SETFONT,reinterpret_cast<WPARAM>(calibrationFont),TRUE);return control;}
RECT calibrationCanvas(){RECT r{80,230,820,610};Config cfg=config();if(!monitors.empty()){MONITORINFO mi{sizeof mi};GetMonitorInfo(monitors[std::min<size_t>(cfg.screen,monitors.size()-1)],&mi);float aspect=float(mi.rcMonitor.right-mi.rcMonitor.left)/(mi.rcMonitor.bottom-mi.rcMonitor.top);int width=std::min(740,int(380*aspect)),height=int(width/aspect);r={450-width/2,420-height/2,450+width/2,420+height/2};}return r;}
POINT calibrationXY(LedPoint p,RECT r){if(calibrationRear)p.x=1-p.x;return {r.left+LONG(p.x*(r.right-r.left)),r.top+LONG(p.y*(r.bottom-r.top))};}
void paintCalibration(HDC dc){RECT client{};GetClientRect(calibrationWindow,&client);FillRect(dc,&client,GetSysColorBrush(COLOR_WINDOW));auto oldFont=SelectObject(dc,calibrationFont);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(35,45,55));RECT r=calibrationCanvas();HBRUSH bg=CreateSolidBrush(RGB(245,248,252));FillRect(dc,&r,bg);DeleteObject(bg);
 HPEN border=CreatePen(PS_SOLID,2,RGB(80,100,120));auto oldPen=SelectObject(dc,border);auto oldBrush=SelectObject(dc,GetStockObject(NULL_BRUSH));Rectangle(dc,r.left,r.top,r.right,r.bottom);SelectObject(dc,oldPen);DeleteObject(border);
 const LedPoint corners[]={{1,1},{1,0},{0,0},{0,1}};
 for(int j=0;j<4;j++){POINT p=calibrationXY(corners[j],r);std::wstring label=std::wstring(calibrationRear?rearCornerNames[j]:frontCornerNames[j])+L" / "+(calibrationCorners[j]?std::to_wstring(calibrationCorners[j]):L"?");RECT text{p.x<450?p.x-60:p.x-20,p.y<420?p.y-26:p.y+15,p.x<450?p.x+110:p.x+130,p.y<420?p.y-4:p.y+40};DrawTextW(dc,label.c_str(),-1,&text,DT_LEFT|DT_SINGLELINE);}
 if(validCorners(calibrationCorners)){auto points=cornerPoints(calibrationCorners);Config cfg=config();auto regions=cornerRegions(calibrationCorners,cfg.depth/100.f,float(r.right-r.left)/(r.bottom-r.top));int selected=g.identifyLed.load();
  auto rect=regions[selected];
  auto drawRegion=[&](int index,bool filled){auto region=regions[index];auto p=calibrationXY({region.x0,region.y0},r),q=calibrationXY({region.x1,region.y1},r);RECT area{std::min(p.x,q.x),std::min(p.y,q.y),std::max(p.x,q.x),std::max(p.y,q.y)};
   bool corner=std::find(calibrationCorners.begin(),calibrationCorners.end(),index+1)!=calibrationCorners.end();
   if(corner){auto c=calibrationXY(points[index],r);int radius=int((r.bottom-r.top)*cfg.depth/100.f);int saved=SaveDC(dc);IntersectClipRect(dc,r.left,r.top,r.right,r.bottom);Ellipse(dc,c.x-radius,c.y-radius,c.x+radius,c.y+radius);RestoreDC(dc,saved);}
   else if(filled)FillRect(dc,&area,(HBRUSH)GetCurrentObject(dc,OBJ_BRUSH));else Rectangle(dc,area.left,area.top,area.right,area.bottom);
  };
  HBRUSH fill=CreateSolidBrush(RGB(199,227,255));SelectObject(dc,fill);drawRegion(selected,true);SelectObject(dc,oldBrush);DeleteObject(fill);
  HPEN boxes=CreatePen(PS_SOLID,1,RGB(183,199,211));SelectObject(dc,boxes);for(int i=0;i<45;i++)drawRegion(i,false);SelectObject(dc,oldPen);DeleteObject(boxes);
  HPEN line=CreatePen(PS_SOLID,2,RGB(49,125,182));SelectObject(dc,line);for(int i=0;i<44;i++){auto p=calibrationXY(points[i],r),q=calibrationXY(points[i+1],r);MoveToEx(dc,p.x,p.y,nullptr);LineTo(dc,q.x,q.y);if(i%4==1){double angle=std::atan2(double(q.y-p.y),double(q.x-p.x));int x=(p.x+q.x)/2,y=(p.y+q.y)/2;MoveToEx(dc,x-LONG(7*std::cos(angle-.5)),y-LONG(7*std::sin(angle-.5)),nullptr);LineTo(dc,x,y);LineTo(dc,x-LONG(7*std::cos(angle+.5)),y-LONG(7*std::sin(angle+.5)));}}SelectObject(dc,oldPen);DeleteObject(line);
  HFONT tiny=CreateFontW(-13,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");SelectObject(dc,tiny);
  for(int i=0;i<45;i++){auto p=calibrationXY(points[i],r);HBRUSH dot=CreateSolidBrush(i==selected?RGB(255,188,60):RGB(255,255,255));SelectObject(dc,dot);Ellipse(dc,p.x-10,p.y-10,p.x+11,p.y+11);SelectObject(dc,oldBrush);DeleteObject(dot);RECT number{p.x-10,p.y-9,p.x+11,p.y+10};auto text=std::to_wstring(i+1);DrawTextW(dc,text.c_str(),-1,&number,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  SelectObject(dc,calibrationFont);DeleteObject(tiny);auto center=calibrationXY(points[selected],r);MoveToEx(dc,center.x-5,center.y,nullptr);LineTo(dc,center.x+6,center.y);MoveToEx(dc,center.x,center.y-5,nullptr);LineTo(dc,center.x,center.y+6);
  RECT gap{r.left,r.bottom-85,r.right,r.bottom-60};DrawTextW(dc,L"1 为输入端，45 为末端；底边中间没有灯，不跨缺口连接。",-1,&gap,DT_CENTER|DT_SINGLELINE);
 }else{RECT text{r.left+40,r.top+70,r.right-40,r.bottom-50};DrawTextW(dc,L"正面走线：底边右侧 → 右下 → 右上 → 左上 → 左下 → 底边左侧\n\n定位四角后，这里显示全部 45 颗灯的位置和采样范围。\n尚未确认的灯珠位置不会用猜测编号填充。",-1,&text,DT_CENTER|DT_WORDBREAK);}
 SelectObject(dc,oldBrush);SelectObject(dc,oldFont);
}
LRESULT CALLBACK calibrationProc(HWND h,UINT msg,WPARAM wp,LPARAM lp){switch(msg){
 case WM_CREATE:{calibrationWindow=h;calibrationFont=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");auto cfg=config();calibrationCorners=cfg.calibrated?cfg.corners:std::array<int,4>{};calibrationRear=false;
  calibrationControl(L"STATIC",L"四角校准 · 所有方向以屏幕正面为准",0,24,18,650,25,0);calibrationControl(L"BUTTON",L"切换正面 / 背面预览",BS_PUSHBUTTON,670,15,200,30,315);
  calibrationControl(L"BUTTON",L"上一颗 ←",BS_PUSHBUTTON,24,60,120,34,301);calibrationNumber=calibrationControl(L"EDIT",L"1",WS_BORDER|ES_NUMBER|ES_CENTER,155,60,60,34,303);calibrationControl(L"STATIC",L"/ 45",0,222,68,50,25,0);calibrationControl(L"BUTTON",L"下一颗 →",BS_PUSHBUTTON,280,60,120,34,302);calibrationControl(L"BUTTON",L"跳到编号",BS_PUSHBUTTON,415,60,110,34,304);calibrationIdentify=calibrationControl(L"BUTTON",L"暂停定位灯",BS_PUSHBUTTON,550,60,160,34,314);
  for(int i=0;i<4;i++)calibrationRecord[i]=calibrationControl(L"BUTTON",L"",BS_PUSHBUTTON,24+i*215,110,205,36,310+i);
  calibrationStatus=calibrationControl(L"STATIC",L"",0,24,155,850,52,0);calibrationControl(L"STATIC",L"宽度按灯位间距；深度向内延伸。角灯仅取方框内以角点为中心的四分之一圆。",0,24,660,860,26,0);calibrationControl(L"BUTTON",L"颜色校准向导",BS_PUSHBUTTON,24,700,180,36,318);calibrationSave=calibrationControl(L"BUTTON",L"保存并应用四角布局",BS_PUSHBUTTON,510,700,220,36,316);calibrationControl(L"BUTTON",L"关闭",BS_PUSHBUTTON,750,700,120,36,317);SetTimer(h,1,1000,nullptr);identification(true);refreshCalibration();return 0;}
 case WM_TIMER:refreshCalibration();return 0;
 case WM_PAINT:{PAINTSTRUCT ps;auto dc=BeginPaint(h,&ps);paintCalibration(dc);EndPaint(h,&ps);return 0;}
 case WM_PRINTCLIENT:paintCalibration(reinterpret_cast<HDC>(wp));return 0;
 case WM_ERASEBKGND:return 1;
 case WM_KEYDOWN:if(wp==VK_LEFT)selectCalibration(g.identifyLed.load());else if(wp==VK_RIGHT)selectCalibration(g.identifyLed.load()+2);return 0;
 case WM_COMMAND:{int id=LOWORD(wp);if(id==301)selectCalibration(g.identifyLed.load());else if(id==302)selectCalibration(g.identifyLed.load()+2);else if(id==304){BOOL valid=false;UINT value=GetDlgItemInt(h,303,&valid,FALSE);if(valid)selectCalibration(int(value));}
  else if(id>=310&&id<=313){calibrationCorners[id-310]=g.identifyLed.load()+1;refreshCalibration();}
  else if(id==314){identification(!g.identifying);refreshCalibration();}
  else if(id==315){calibrationRear=!calibrationRear;SetWindowTextW(h,calibrationRear?L"四角校准 · 背面镜像预览":L"四角校准 · 正面视角");refreshCalibration();}
  else if(id==316&&validCorners(calibrationCorners)){Config c=config();c.corners=calibrationCorners;c.calibrated=true;c.reverse=0;c.offset=0;c.right=c.corners[1]-c.corners[0]+1;c.top=c.corners[2]-c.corners[1];c.left=c.corners[3]-c.corners[2];c.bottom=45-c.left-c.top-c.right;{std::lock_guard guard(g.lock);g.config=c;g.hasColors=false;}g.revision++;save();if(IsWindow(reverseBox)){SendMessage(reverseBox,BM_SETCHECK,BST_UNCHECKED,0);SetWindowTextW(offsetEdit,L"0");EnableWindow(reverseBox,FALSE);EnableWindow(offsetEdit,FALSE);int counts[]={c.left,c.top,c.right,c.bottom};for(int i=0;i<4;i++)SetWindowTextW(sideEdits[i],std::to_wstring(counts[i]).c_str());}identification(false);refreshCalibration();MessageBoxW(h,L"四角布局已保存。主程序将使用这些采样区域；底边缺口保留。",L"已保存",MB_OK);}
  else if(id==318)openVisualWizard();else if(id==317)DestroyWindow(h);return 0;}
 case WM_CLOSE:DestroyWindow(h);return 0;
 case WM_DESTROY:identification(false);KillTimer(h,1);calibrationWindow=nullptr;if(calibrationFont){DeleteObject(calibrationFont);calibrationFont=nullptr;}if(calibrationOwnLoop)PostQuitMessage(0);return 0;
 }return DefWindowProcW(h,msg,wp,lp);}
void openCalibration(bool standalone=false){if(IsWindow(calibrationWindow)){ShowWindow(calibrationWindow,SW_SHOW);SetForegroundWindow(calibrationWindow);return;}calibrationOwnLoop=standalone;WNDCLASSW wc{};wc.lpfnWndProc=calibrationProc;wc.hInstance=GetModuleHandle(nullptr);wc.lpszClassName=L"ScreenGlowCalibration";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);RegisterClassW(&wc);HWND h=CreateWindowW(wc.lpszClassName,L"四角校准 · 正面视角",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,920,800,nullptr,nullptr,wc.hInstance,nullptr);if(h)ShowWindow(h,SW_SHOW);else if(standalone)PostQuitMessage(1);}







