#pragma once
inline int startupCommand(const wchar_t*cmd){
 bool enable=wcsstr(cmd,L"--install-startup")!=nullptr,disable=wcsstr(cmd,L"--remove-startup")!=nullptr;
 if(wcsstr(cmd,L"--exit")){if(auto h=FindWindowW(L"ScreenGlowWindow",nullptr)){DWORD_PTR result=0;return SendMessageTimeoutW(h,WM_COMMAND,102,0,SMTO_ABORTIFHUNG,3000,&result)?0:1;}return 0;}
 if(!enable&&!disable)return -1;HKEY key=nullptr;auto status=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);if(status!=ERROR_SUCCESS)return int(status);
 if(enable){wchar_t path[32768];DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n>=32768){RegCloseKey(key);return 1;}std::wstring value=L"\""+std::wstring(path)+L"\" --tray";status=RegSetValueExW(key,L"AnsnakeScreenGlow",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));}
 else {status=RegDeleteValueW(key,L"AnsnakeScreenGlow");if(status==ERROR_FILE_NOT_FOUND)status=ERROR_SUCCESS;}RegCloseKey(key);return int(status);
}
