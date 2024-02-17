#pragma once

#include "wdwm.h"
#include <winuser.h>

/* tagging */
static const char *tags[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9"};
/* the gaps between windows can be adjusted by tuning borderpx. */
static const int borderpx = -5;
static const int mouse_warp = 1;

/* not really a configuration. Just a hack to treat windows task bar like the dwm bar */
static const int showbar = 1;
static const int topbar = 0;

static const Layout layouts[] = {
    /* symbol     arrange function */
    {"[]=", tilewide}, /* first entry is default */
    {"><>", NULL},     /* no layout function means floating behavior */
    {"[M]", monocle},
};

#define MODKEY MetaMask
// clang-format off
#define TAGKEYS(KEY,TAG) \
	{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
	{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
	{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

// clang-format on

static const char termcmd[] = "alacritty.exe";

static const Key keys[] =
{
	/* modifier                     key             function                 argument */
	{ MODKEY|ShiftMask,             VK_RETURN,      spawn,                {.v = termcmd } },
	{ MODKEY|ControlMask,           'J',            focusstack,           {.i = +1 } },
	{ MODKEY|ControlMask,           'K',            focusstack,           {.i = -1 } },
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
	{ MODKEY,                       'W',            setlayout,            {.v = &layouts[2]} },
	{ MODKEY,                       VK_SPACE,       setlayout,            {0} },
	{ MODKEY|ShiftMask,             VK_SPACE,       togglefloating,       {0} },
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
	TAGKEYS('9', 7)
};
