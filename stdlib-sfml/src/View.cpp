#include <math.h>
#include <SFML/Window.hpp>

#include "openTuringLibDefs.h"
#include "WindowManager.h"
#include "RGB.h"

static sf::Color clearColor(255,255,255);

extern "C" {
    void Turing_StdlibSFML_View_Cls() {
    	WinMan->clearWin(WinMan->curWinID());
    }
    void Turing_StdlibSFML_View_Update() {
        WinMan->updateWindow(WinMan->curWinID(),true);
    }
    void Turing_StdlibSFML_View_Set(TString *format) {
        WinMan->setWinParams(WinMan->curWinID(),format->strdata);
    }
}