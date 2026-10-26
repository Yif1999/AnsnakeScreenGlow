#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <psapi.h>
#include <cstdio>
#include <vector>
#include <chrono>
#include <algorithm>

using namespace winrt;
using namespace winrt::Windows::Graphics::Capture;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

double ms() { static auto start=std::chrono::steady_clock::now(); return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); }

void memory(IDXGIFactory1* factory) {
    for(UINT i=0;;++i){com_ptr<IDXGIAdapter1>a;if(factory->EnumAdapters1(i,a.put())==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 d{};a->GetDesc1(&d);auto a3=a.try_as<IDXGIAdapter3>();
        if(a3){DXGI_QUERY_VIDEO_MEMORY_INFO local{},shared{};a3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&local);a3->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,&shared);
            std::printf("GPU %ls process local=%.2fMiB nonlocal=%.2fMiB\n",d.Description,local.CurrentUsage/1048576.,shared.CurrentUsage/1048576.);}}
    PROCESS_MEMORY_COUNTERS_EX p{};GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&p),sizeof p);
    std::printf("RAM working=%.2fMiB private=%.2fMiB\n",p.WorkingSetSize/1048576.,p.PrivateUsage/1048576.);
}

com_ptr<ID3D11ComputeShader> shader(ID3D11Device* d,const char* code){
 com_ptr<ID3DBlob>b,e;HRESULT hr=D3DCompile(code,strlen(code),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,b.put(),e.put());
 if(FAILED(hr)){if(e)std::printf("%s\n",static_cast<char*>(e->GetBufferPointer()));check_hresult(hr);}
 com_ptr<ID3D11ComputeShader>s;check_hresult(d->CreateComputeShader(b->GetBufferPointer(),b->GetBufferSize(),nullptr,s.put()));return s;
}





