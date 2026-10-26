#define wWinMain unusedApplicationEntry
#include "ScreenGlow.cpp"
#include <cassert>
uint64_t frames(){std::lock_guard guard(g.lock);return g.captured;}
int main(){
 SetProcessDPIAware();EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR m,HDC,LPRECT,LPARAM)->BOOL{monitors.push_back(m);return TRUE;},0);assert(!monitors.empty());
 {std::lock_guard guard(g.lock);g.config.calibrated=true;g.config.corners={4,14,31,41};}
 Handle ready(CreateEventW(nullptr,FALSE,FALSE,nullptr));g.colorsReady=ready.h;std::thread capture(captureWorker);
 bool passed=true;
 for(int cycle=0;cycle<6;cycle++){
  auto before=frames();Sleep(2000);auto after=frames();if(after-before<15){std::puts("FAIL capture did not resume");passed=false;break;}
  if(cycle%3==0)g.enabled=false;
  else if(cycle%3==1)g.identifying=true;
  else {std::lock_guard guard(g.lock);g.config.mode=1;}
  g.revision++;Sleep(700);auto paused=frames();Sleep(500);if(frames()!=paused){std::puts("FAIL capture advanced while paused");passed=false;break;}
  g.enabled=true;g.identifying=false;{std::lock_guard guard(g.lock);g.config.mode=0;g.config.depth=cycle%2?12:30;}g.revision++;
  std::printf("PASS cycle %d: %llu active frames, stable while suspended\n",cycle,(unsigned long long)(after-before));
 }
 g.run=false;capture.join();g.colorsReady=nullptr;std::puts(passed?"PASS capture lifecycle: pause/resume, identification, static mode, 12%/30% depth rebuild, clean join; no USB":"FAIL lifecycle");return passed?0:1;
}

