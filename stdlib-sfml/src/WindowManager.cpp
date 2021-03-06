#include "WindowManager.h"

#include <sstream>
#include <algorithm>
#include <iterator>
#include <iostream>

#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include "TuringCommon/RuntimeError.h"

WindowManager *WinMan = NULL;
sf::Clock eventClock;



void Turing_StdlibSFML_Window_Init() {
    if (WinMan == NULL) {
        WinMan = new WindowManager();
    } else {
        TuringCommon::runtimeError("Window init called when there is already a window.");
    }
}

void Turing_StdlibSFML_Window_Cleanup() {
    if (WinMan != NULL) {
        delete WinMan;
        WinMan = NULL;
    } else {
        TuringCommon::runtimeError("Window cleanup called when there is no window to clean up.");
    }
}

extern "C" {
    // called every couple of lines. Use it to check for events
    void Turing_StdlibSFML_PeriodicCallback() {
        // check every quarter of a second. This does make input delayed.
        // It only stops the system from deeming us unresponsive
        if(WinMan && eventClock.GetElapsedTime() > 0.5) {
            WinMan->surface();
            eventClock.Reset();
        }
    }
}

WindowManager::WindowManager() : Windows("Window"), CurWin(0){
    //Settings.AntialiasingLevel = 2;  // Request 2 levels of antialiasing
    
    MainWin = newWin("");
    setCurWin(MainWin);
}

WindowManager::~WindowManager() {}

TuringWindow *WindowManager::curWin() {
    return Windows.get(CurWin);
}

TuringWindow *WindowManager::getWin(TInt winId) {
    return Windows.get(winId);
}

void WindowManager::setCurWin(TInt winId) {
    TuringWindow *win = getWin(winId);
    win->Win.SetActive(true);
    CurWin = winId;
}

TInt WindowManager::curWinID() {
    return CurWin;
}

TInt WindowManager::newWin(const std::string &params) {
    TInt id = Windows.getNew();
    TuringWindow *newWin = Windows.get(id);
    
    setWinParams(id, params);
    newWin->Win.UseVerticalSync(true);
    // clear both buffers
    clearWin(id);
    updateWindow(id, true);
    clearWin(id);
    // activate
    newWin->Win.Show(true);
    setCurWin(id);
    
    return id;
}

void WindowManager::closeWin(TInt winId) {
    if (winId == 0) {
        TuringCommon::runtimeError("Can't close the main window.");
    }
    Windows.remove(winId);
    
    // set the active window to the main one if we just closed it
    if (CurWin == winId) {
        CurWin = MainWin;
    }
}

// this uses strings and vectors heavily so it is OK to use 'using'
void WindowManager::setWinParams(TInt winId, const std::string &params) {
    TuringWindow* win = getWin(winId);
    
    std::vector<std::string> items;
    WindowManager::split(items, params, ",");
    
    std::vector<std::string>::const_iterator item = items.begin();
    for (; item != items.end(); ++item) {
        std::vector<std::string> parts;
        split(parts,*item, ":;"); // split by either
        if (parts.size() == 0) TuringCommon::runtimeError("Malformed window format specifier component.");
        
        std::string tagname = parts[0];
        
        if ((tagname.compare("graphics") == 0 || tagname.compare("screen") == 0) &&
            parts.size() == 3) { // graphics:x;y
            int x = atoi(parts[1].c_str());
            int y = atoi(parts[2].c_str());
            if (x < 1 || y < 1) {
                TuringCommon::runtimeError("Tried to create a window with negative or zero size");
            }
            win->Width  = x;
            win->Height = y;
            win->Win.SetSize(x,y);
        } else if (tagname.compare("title") == 0 && parts.size() == 2) { // title:name
            win->Title = parts[1];
        } else if (tagname.compare("offscreenonly") == 0) {
            win->OffScreenOnly = true;
        } else if (tagname.compare("nooffscreenonly") == 0) {
            win->OffScreenOnly = false;
        } else {
            TuringCommon::runtimeError("Don't recognize window option. Might just not be implemented.",true); // true = warn
        }
    }
    
    win->Win.Create(sf::VideoMode(win->Width,win->Height), win->Title,sf::Style::Close,Settings);
    
    //set up OpenGL
    setupOpenGL(win);
//    clearWin(winId);
}

void WindowManager::setupOpenGL(TuringWindow *win) {
    win->Win.SetActive();
    win->Win.PreserveOpenGLStates(true);
    //glViewport( 0, 0, win->Width, win->Height );
    glMatrixMode (GL_PROJECTION);
    glLoadIdentity ();
    glOrtho (0, win->Width, 0, win->Height, -1, 1);
    glDisable(GL_DEPTH_TEST);
    glMatrixMode (GL_MODELVIEW);
    glLoadIdentity();
    // Displacement trick for exact pixelization
    glTranslatef(0.375, 0.375, 0);
    curWin()->Win.SetActive();
}

void WindowManager::updateWindow(TInt winId, bool force) {
    TuringWindow* win = getWin(winId);
    if (force || !win->OffScreenOnly) {
        doWinUpdate(win);
    }
}

void WindowManager::updateCurWin() {
    TuringWindow* win = curWin();
    if (!win->OffScreenOnly) {
        surface();
        doWinUpdate(win);
        
    }
}

void WindowManager::clearWin(TInt winId) {
    TuringWindow *win = getWin(winId);
    win->Win.Clear(sf::Color(255,255,255));
    win->PutLine = 1;
}

void WindowManager::surface() {
    sf::Event Event;
    std::map<TInt, TuringWindow*>::iterator iter;
    // process all events for every window
    for(iter=Windows.Items.begin(); iter != Windows.Items.end(); ++iter) {
        TuringWindow *cur = iter->second;
        TInt curId = iter->first;
        while (cur->Win.GetEvent(Event))
        {
            switch (Event.Type) {
                case sf::Event::Closed:
                    if (curId == MainWin) {
                        std::cout << "Main Window Closed" << std::endl;
                        exit(0); // we're done here, user closed main window
                    }
                    break;
                case sf::Event::MouseMoved:
                    cur->MouseX = Event.MouseMove.X;
                    cur->MouseY = Event.MouseMove.Y;
                    break;
                case sf::Event::MouseButtonPressed:
                    cur->MouseX = Event.MouseButton.X;
                    cur->MouseY = Event.MouseButton.Y;
                    cur->buttonEvent(Event.MouseButton.Button, true);
                    break;
                case sf::Event::MouseButtonReleased:
                    cur->buttonEvent(Event.MouseButton.Button, false);
                    break;
                case sf::Event::KeyPressed:
                    cur->keyEvent(Event.Key.Code, true);
                    break;
                case sf::Event::KeyReleased:
                    cur->keyEvent(Event.Key.Code, false);
                    break;
                    
                default:
                    break;
            }
        }
    }
}

#pragma mark Protected and Private Methods

void WindowManager::doWinUpdate(TuringWindow *win) {
    void *pixBuf = malloc(win->Width * win->Height * sizeof(GLfloat) * 4);
    glReadPixels(0, 0, win->Width, win->Width, GL_RGBA, GL_FLOAT, pixBuf);
    win->Win.Display();
    win->Win.Clear(sf::Color(255,255,255));
    glWindowPos2i(0, 0);
    glDrawPixels(win->Width, win->Height, GL_RGBA, GL_FLOAT, pixBuf);
    free(pixBuf);
    // recalculate fps
    float frameTime = win->FrameClock.GetElapsedTime();
    win->FrameClock.Reset();;
    float newFps = win->Fps*0.95 + (1.0/frameTime)*0.05;
    win->Fps = newFps;
}

void WindowManager::split(std::vector<std::string>& lst, const std::string& input, const std::string& separators, bool remove_empty)
{
    std::ostringstream word;
    for (size_t n = 0; n < input.size(); ++n)
    {
        if (std::string::npos == separators.find(input[n]))
            word << input[n];
        else
        {
            if (!word.str().empty() || !remove_empty)
                lst.push_back(word.str());
            word.str("");
        }
    }
    if (!word.str().empty() || !remove_empty)
        lst.push_back(word.str());
}
