#include <windows.h>
#include <commctrl.h>
#include <ShellScalingApi.h>
#include <sstream>
#include <string>
#include <winternl.h>
#include <uxtheme.h>
#include <vssym32.h>

#include "GlassWindow.h"
#include "GlassWindowApp.h"
#include "UniqueHandle.h"

#include "odbs.h"

using namespace std;
using namespace aspk;

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine,
         int nCmdShow)
{
  GlassWindowApp app;
  if (!app) {
    MessageBox(NULL, L"GlassWindowApp failed to initialize", L"Error", MB_OK | MB_ICONSTOP);
    return 1;
  }

  GlassWindow::Params params;
  params.SetTitleText(L"Scratch Program");
  params.SetFlags(GlassWindow::Params::eSolidGlass |
                  GlassWindow::Params::eQuitOnDestroy);

  GlassWindow mainWindow(hInstance, params);
  mainWindow.Show(nCmdShow);
  mainWindow.Update();

  return app.Run();
}

