cl -Zi -EHsc -DNTDDI_VERSION=0x06030000 -D_WIN32_WINNT=0x0A00 -DUNICODE -D_UNICODE glass.cpp DpiScaler.cpp GlassWindow.cpp GlassWindowApp.cpp Listview.cpp user32.lib gdi32.lib dwmapi.lib uxtheme.lib comctl32.lib gdiplus.lib msimg32.lib
mt -manifest glass.exe.manifest -outputresource:glass.exe;#1
