* multi-monitor not functional
* spawned processes close when the parent closes
* changing focus causes flicker due to HWND_BOTTOM
* windows that override borders / decorators  become ugly when style is overridden 
- Firefox / Windows Terminal have settings that disable this behavior, but it would be best to find a generic way to detect this and figure out a good way to handle it. 
a. Can we detect if a Window is drawing in the caption / border area? 
    1. Can we check if the size of a window's client area is equal to the windows entire rect?
b. Can we disguise the ugliness by creating a solid border to indicate which window is focused?
c. If a window uses native title bar and borders, it will not fill its entire rect. Can we ensure it does so without disabling the borders completely?
* There is no visual indication which window is focused
* floating windows are hard to distinguish due to no borders (enable border style for floating windows as temp?)
* weird behavior when KS8400 creates child windows
* when a window has a modal dialog (e.g. confirm button from smartinstaller), and the main window loses focus, the modal dialog can become covered by tiled winodws
- How can we ensure tiled winodws never leave the bottom of the Z order???
* While Rider is debugging, stuff stops working
* Multiple Rider solutions are weird
* Look into AnimateWindows API
* Learn how Windows actually does hierarchies:
- https://www.cnblogs.com/fwycmengsoft/p/10201734.html
- https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features?redirectedfrom=MSDN#owned_windows
