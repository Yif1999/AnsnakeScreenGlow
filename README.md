# AnsnakeScreenGlow

幻蛇 P5（1.5 米、45 灯）USB 屏幕背光控制程序，非官方项目。原生 C++ / Win32，面向低延迟、低开销的 60 Hz 屏幕追光。

## 使用

解压到固定的可写目录，退出官方软件，运行 `ScreenGlow.exe`。当前要求屏幕连接 NVIDIA 显卡，开发测试环境为 Windows 11。使用 Windows Graphics Capture 无边框采集，系统不支持取消边框时报告错误。

通过布局校准定位正面视角的右下、右上、左上、左下四颗角灯，再按肉眼色彩向导调整。也可单独运行 `Calibrate.exe`。关闭主窗口收进托盘，从托盘菜单退出。

唯一算法为原图全像素边缘高斯：普通灯区宽度等权，仅向屏幕内侧衰减；角灯使用以屏幕角点为中心的四分之一圆高斯。无降采样纹理，无稀疏采样。输出保留相邻灯 25% / 50% / 25% 混合。

## 登录自启

在程序目录运行 `ScreenGlow.exe --install-startup`，当前用户登录后以托盘方式启动。移动目录后重新执行。取消使用 `ScreenGlow.exe --remove-startup`。

## 配置与升级

程序目录的 `ScreenGlow.ini` 保存布局和亮度，`LedVisual.profile` 保存肉眼校准，`LedColor.profile` 是可选设备测量配置。升级保留这些文件。发行包不含开发者的个人配置。

## 构建

安装 Visual Studio 的“使用 C++ 的桌面开发”和 Windows SDK。

```powershell
.\build.cmd -Tests
.\build\release\CornerLayoutTests.exe
.\build\release\LedColorTests.exe
.\build\release\EdgeGaussianTests.exe
.\build\release\CalibrationTests.exe
.\build\release\CaptureLifecycleTests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\Package.ps1
```

程序输出到 `build/release`，发行包输出到 `dist`。GPU 测试需要 D3D11，采集测试需要可采集的 NVIDIA 屏幕，测试不访问 USB。CI 执行离线几何和色彩测试。

详见 [算法](docs/algorithm.md) 与 [校准](docs/calibration.md)。仅验证过 P5 的 45 灯配置，其他型号尚未验证。
