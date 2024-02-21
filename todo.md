* multi-monitor not really functional
* spawned processes close when the parent closes
* managed windows are raised on top of unmanaged windows
* windows that override borders / decorators  become ugly when style is overridden 
- Firefox / Windows Terminal have settings that disable this behavior, but it would be best to find a generic way to detect this and figure out a good way to handle it. 
a. Can we detect if a Window is drawing in the caption / border area? 
    1. Can we check if the size of a window's client area is equal to the windows entire rect?
b. Can we disguise the ugliness by creating a solid border to indicate which window is focused?
c. If a window uses native title bar and borders, it will not fill its entire rect. Can we ensure it does so without disabling the borders completely?
* There is no visual indication which window is focused
