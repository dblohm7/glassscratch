cl -Zi -EHsc -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0603 -DUNICODE -D_UNICODE glass.cpp glasswnd.cpp user32.lib gdi32.lib dwmapi.lib uxtheme.lib comctl32.lib
mt -manifest glass.exe.manifest -outputresource:glass.exe;#1
