#pragma once

#include "util.h"
#include "wdwm.h"
#include <winuser.h>

/* tagging */
static const char *tags[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
/* the gaps between windows can be adjusted by tuning borderpx. */
static const int borderpx = 0;
static const int gaps = 4;
static const float mfact = 0.65;     /* factor of master area size [0.05..0.95] */
static const int nmaster = 2;        /* number of clients in master area */
static const int resizehints = 1;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 1; /* 1 will force focus on the fullscreen window */
/* not really a configuration. Just a hack to treat windows task bar like the dwm bar */
static const int showbar = 1;
static const int topbar = 0;

int mouse_warp = 0;
static const int mouse_focus = 0;
static const int mouse_drag = 1;
static const int mouse_resize = 1;

enum { STARTSWITH, ENDSWITH, MATCHES, CONTAINS };
typedef struct {
  char *title;
  int matchtype;
} unmanaged_t;

static const unmanaged_t unmanaged[] = {
    {"Microsoft Teams Notification", MATCHES},
    {"OleMainThreadWndName", MATCHES},
    {"Task Manager", MATCHES},
    {"Default IME", MATCHES},
    {"Windows Input Experience", MATCHES},
    {"PopupHost", MATCHES},
    {"AgilentLicenseNotifier", MATCHES},
    {"Task Switching", MATCHES},
    {"Program Manager", MATCHES},
    {"Windows Requested Elevation: ", STARTSWITH},
};

static const Layout layouts[] = {
    /* symbol     arrange function */
    {"[]=", tilewide}, /* first entry is default */
    {"><>", NULL},     /* no layout function means floating behavior */
                       // {"[M]", monocle},
};

#define MODKEY MetaMask
// clang-format off
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },


// ShellExecute needs spaces to be quoted by double double quoting strings
#define QUOTE(X) #X
#define DQUOTE(X) "\""#X"\""
static const char termcmd[] = DQUOTE("C:\\Program Files\\WindowsApps\\Microsoft.WindowsTerminal_1.18.10301.0_x64__8wekyb3d8bbwe\\WindowsTerminal.exe");

// clang-format off
static const Key keys[] =
{
	/* modifier                     key             function                 argument */
	{ MODKEY|ShiftMask,             VK_RETURN,      spawn,                {.v = termcmd } },
	{ MODKEY,                       'J',            focusstack,           {.i = +1 } },
	{ MODKEY,                       'K',            focusstack,           {.i = -1 } },
	{ MODKEY|ShiftMask,             'J',            pushdown,             {0} },
	{ MODKEY|ShiftMask,             'K',            pushup,               {0} },
	{ MODKEY,                       'I',            incnmaster,           {.i = +1 } },
	{ MODKEY,                       'O',            incnmaster,           {.i = -1 } },
	{ MODKEY,                       'H',            setmfact,             {.f = -0.05} },
	{ MODKEY,                       'L',            setmfact,             {.f = +0.05} },
	{ MODKEY,                       'G',            zoom,                 {0} },
	{ MODKEY,                       'Q',            killclient,           {0} },
	{ MODKEY,                       'T',            setlayout,            {.v = &layouts[0]} },
	{ MODKEY,                       'M',            setlayout,            {.v = &layouts[1]} },
	{ MODKEY|ShiftMask,             VK_SPACE,       togglefloating,       {0} },
	// { MODKEY,                       'W',            setlayout,            {.v = &layouts[1]} },
	{ MODKEY,                       '0',            view,                 {.ui = ~0 } },
	{ MODKEY|ShiftMask,             '0',            toggleview,           {.ui = ~0 } },
	{ MODKEY,                       VK_OEM_PERIOD,  focusmon,             {.i = -1 } },
	{ MODKEY,                       VK_OEM_COMMA,   focusmon,             {.i = +1 } },
	{ MODKEY|ShiftMask,             VK_OEM_PERIOD,  tagmon,               {.i = -1 } },
	{ MODKEY|ShiftMask,             VK_OEM_COMMA,   tagmon,               {.i = +1 } },
	{ MODKEY|ShiftMask,             'Q',            quit,                 {0} },
	TAGKEYS('1', 0)
	TAGKEYS('2', 1)
	TAGKEYS('3', 2)
	TAGKEYS('4', 3)
	TAGKEYS('5', 4)
	TAGKEYS('6', 5)
	TAGKEYS('7', 6)
	TAGKEYS('8', 7)
	TAGKEYS('9', 8)
};

