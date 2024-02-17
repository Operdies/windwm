#include "wdwm.h"
#include "config.h"
#include "util.h"
#include <dbghelp.h>
#include <shellapi.h>
#include <string.h>

int barheight = 0;

void _spawn(const char *cmd, bool elevate) {
  if (!ShellExecuteA(NULL, elevate ? "runas" : "open", cmd, 0, 0, SW_SHOWNORMAL))
    errormsg("Failed to spawn %s%s:", elevate ? "elevated " : "", cmd);
}
void spawn(const Arg *arg) { _spawn(arg->v, false); }
void spawn_elevated(const Arg *arg) { _spawn(arg->v, true); }

void focusclientundercursor(int x, int y) {
  Client *c;
  Monitor *m;
  for (m = mons; m; m = m->next) {
    for (c = m->clients; c; c = c->next)
      if (x >= c->x && x <= c->x + c->w && y >= c->y && y <= c->y + c->w) {
        if (selmon->sel != c) {
          focus(c);
        }
        return;
      }
  }
}

bool mouseenabled = false;
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (mouseenabled && nCode == HC_ACTION) {
    PMSLLHOOKSTRUCT p = (PMSLLHOOKSTRUCT)lParam;
    TRACEF("Mouse event: %lld %ld %ld", wParam, p->pt.x, p->pt.y);
    focusclientundercursor(p->pt.x, p->pt.y);
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
}

#define WIN_DOWN (keystate[VK_LWIN] || keystate[VK_RWIN])
#define ALT_DOWN (keystate[VK_LMENU] || keystate[VK_RMENU] || keystate[VK_MENU])
#define CTRL_DOWN (keystate[VK_LCONTROL] || keystate[VK_RCONTROL] || keystate[VK_CONTROL])
#define SHIFT_DOWN (keystate[VK_LSHIFT] || keystate[VK_RSHIFT] || keystate[VK_SHIFT])
#define MODMAP (u8)((WIN_DOWN ? SuperMask : 0) | (ALT_DOWN ? MetaMask : 0) | (CTRL_DOWN ? ControlMask : 0) | (SHIFT_DOWN ? ShiftMask : 0))

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
  static bool keystate[0xff] = {0};
  int intercept = 0;
  if (nCode >= 0) {
    bool is_release, keydown, prevstate;
    unsigned char vk;
    PKBDLLHOOKSTRUCT p;

    // vk max should already be 0xFE, but we mask it here just to be safe
    p = (PKBDLLHOOKSTRUCT)lParam;
    vk = p->vkCode & 0xff;
    prevstate = keystate[(int)vk];
    keydown = 0 == (p->flags & LLKHF_UP);
    is_release = !keydown && prevstate;

    keystate[vk] = keydown;

    u8 modmask = MODMAP;

    for (int i = 0; i < LENGTH(keys); i++) {
      const Key *k = &keys[i];
      if (k->mods == modmask && k->vk == vk) {
        intercept = 1;
        TRACEF("Intercepted key: %d %d %d %d", k->mods, k->vk, keydown, is_release);
        if (is_release)
          k->func(&k->arg);
      }
    }
  }
  return intercept || CallNextHookEx(NULL, nCode, wParam, lParam);
}

bool CanMoveWindow(HWND hwnd);
bool IsWindowResizableMovable(HWND hwnd);

Monitor *createmon(void) {
  Monitor *m;
  m = ecalloc(1, sizeof(Monitor));
  m->tagset[0] = m->tagset[1] = 1;
  m->mfact = mfact;
  m->nmaster = nmaster;
  m->showbar = showbar;
  m->topbar = topbar;
  m->lt[0] = &layouts[0];
  m->lt[1] = &layouts[1 % LENGTH(layouts)];
  return m;
}

void describemonitor(Monitor *m) {
  printf("Monitor: %s\n", m->dd.DeviceName);
  printf("\tResolution: %d x %d\n", m->mw, m->mh);
  printf("\tPosition: %d x %d\n", m->mx, m->my);
  printf("\tWork area: %d x %d\n", m->ww, m->wh);
  printf("\tWork position: %d x %d\n", m->wx, m->wy);
}
void describe_monitors(void) {
  Monitor *m;
  for (m = mons; m; m = m->next) {
    describemonitor(m);
  }
}

DWORD GetProcessIntegrityLevel(HANDLE hProcess) {
  DWORD dwIntegrityLevel = 0;
  HANDLE hToken = NULL;

  if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
    return dwIntegrityLevel;

  DWORD dwLengthNeeded;
  if (!GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwLengthNeeded) && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    PTOKEN_MANDATORY_LABEL pTIL = (PTOKEN_MANDATORY_LABEL)LocalAlloc(0, dwLengthNeeded);
    if (pTIL != NULL) {
      if (GetTokenInformation(hToken, TokenIntegrityLevel, pTIL, dwLengthNeeded, &dwLengthNeeded)) {
        dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid, *GetSidSubAuthorityCount(pTIL->Label.Sid) - 1);
      }
      LocalFree(pTIL);
    }
  }

  CloseHandle(hToken);
  return dwIntegrityLevel;
}

bool IsWindowResizableMovable(HWND hwnd) {
  LONG style = GetWindowLong(hwnd, GWL_STYLE);
  LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

  // Check if window is visible and not minimized
  if ((style & WS_VISIBLE) && !(style & WS_MINIMIZE)) {
    // Check if window is sizable or has a border
    if ((style & WS_SIZEBOX) || (style & WS_BORDER)) {
      // Check if window is not a child, popup, or dialog
      if (!(style & WS_CHILD) && !(style & WS_POPUP) && !(exStyle & WS_EX_DLGMODALFRAME)) {
        return true;
      }
    }
  }
  return false;
}

bool CanMoveWindow(HWND hwnd) {
  DWORD dwProcessId;
  GetWindowThreadProcessId(hwnd, &dwProcessId);

  HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwProcessId);
  if (hProcess != NULL) {
    DWORD dwIntegrityLevel = GetProcessIntegrityLevel(hProcess);
    // Check if the process is not elevated
    if (dwIntegrityLevel == SECURITY_MANDATORY_MEDIUM_RID || dwIntegrityLevel == SECURITY_MANDATORY_LOW_RID) {
      CloseHandle(hProcess);
      return true;
    }
    CloseHandle(hProcess);
  }
  return false;
}

void describeclient(Client *c) {
  printf("Client: %s\n", c->name);
  printf("\tPosition: %d x %d\n", c->x, c->y);
  printf("\tSize: %d x %d\n", c->w, c->h);
  printf("\tMonitor: %s\n", c->mon->dd.DeviceName);
}

void applyrules(Client *c) {
  c->tags = 0;
  c->isfloating = 0;
  c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
}

void resize(Client *c, int x, int y, int w, int h, int interact) { resizeclient(c, x, y, w, h); }

void resizeclient(Client *c, int x, int y, int w, int h) {
  if (c->x == x && c->y == y && c->w == w && c->h == h)
    return;
  c->oldx = c->x;
  c->oldy = c->y;
  c->oldw = c->w;
  c->oldh = c->h;
  c->x = x;
  c->y = y;
  c->w = w;
  c->h = h;
  SetWindowPos(c->hwnd, NULL, c->x, c->y, c->w, c->h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void warpmouse(int x, int y) { SetCursorPos(x, y); }

void warp(const Client *c) {
  POINT p;
  p.x = c->x + c->w / 2;
  p.y = c->y + c->h / 2;
  warpmouse(p.x, p.y);
}

void attachtop(Client *c) {
  int n;
  Monitor *m = selmon;
  Client *below;

  for (n = 1, below = c->mon->clients; below && below->next && (below->isfloating || !ISVISIBLEONTAG(below, c->tags) || n != m->nmaster);
       n = below->isfloating || !ISVISIBLEONTAG(below, c->tags) ? n + 0 : n + 1, below = below->next)
    ;
  c->next = NULL;
  if (below) {
    c->next = below->next;
    below->next = c;
  } else
    c->mon->clients = c;
}

void attachstack(Client *c) {
  c->snext = c->mon->stack;
  c->mon->stack = c;
}

void setwindowpos(HWND hwnd, HWND hwndInsertAfter, int x, int y, int w, int h, UINT flags) {
  TRACEF("Setting window position: (%d,%d) %dx%d", x, y, w, h);
  SetWindowPos(hwnd, hwndInsertAfter, x, y, w, h, flags);
}

void showhide(Client *c) {
  if (!c)
    return;
  if (ISVISIBLE(c)) {
    TRACEF("Showing client: %s", c->name);
    /* show clients top down */
    setwindowpos(c->hwnd, HWND_TOP, c->x, c->y, c->w, c->h, SWP_NOACTIVATE);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    TRACEF("Hiding client: %s", c->name);
    /* hide clients bottom up */
    showhide(c->snext);
    SetWindowPos(c->hwnd, HWND_BOTTOM, c->x, c->y, c->w, c->h, SWP_NOACTIVATE);
  }
}

void arrange(Monitor *m) {
  if (m)
    showhide(m->stack);
  else
    for (m = mons; m; m = m->next)
      showhide(m->stack);
  if (m) {
    arrangemon(m);
    restack(m);
  } else
    for (m = mons; m; m = m->next)
      arrangemon(m);
}

Client *nexttiled(Client *c) {
  for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next)
    ;
  return c;
}

void pop(Client *c) {
  detach(c);
  attach(c);
  focus(c);
  arrange(c->mon);
}

Client *prevtiled(Client *c) {
  Client *p, *r;

  for (p = selmon->clients, r = NULL; p && p != c; p = p->next)
    if (!p->isfloating && ISVISIBLE(p))
      r = p;
  return r;
}

void arrangemon(Monitor *m) {
  if (m->lt[m->sellt]->arrange)
    m->lt[m->sellt]->arrange(m);
}

void attach(Client *c) {
  c->next = c->mon->clients;
  c->mon->clients = c;
}

void seturgent(Client *c, int urg) {
  if (urg && !c->isurgent) {
    c->isurgent = 1;
    c->bw = 2;
  } else if (!urg && c->isurgent) {
    c->isurgent = 0;
    c->bw = borderpx;
  }
}

void toggletag(const Arg *arg) {
  unsigned int newtags;

  if (!selmon->sel)
    return;
  newtags = selmon->sel->tags ^ (arg->ui & TAGMASK);
  if (newtags) {
    selmon->sel->tags = newtags;
    focus(NULL);
    arrange(selmon);
  }
}

void tag(const Arg *arg) {
  if (selmon->sel && arg->ui & TAGMASK) {
    selmon->sel->tags = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
  }
}

void tagmon(const Arg *arg) {
  if (!selmon->sel || !mons->next)
    return;
  sendmon(selmon->sel, dirtomon(arg->i));
}

void toggleview(const Arg *arg) {
  unsigned int newtagset = selmon->tagset[selmon->seltags] ^ (arg->ui & TAGMASK);

  if (newtagset) {
    selmon->tagset[selmon->seltags] = newtagset;
    focus(NULL);
    arrange(selmon);
  }
}

void view(const Arg *arg) {
  if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
    return;
  selmon->seltags ^= 1; /* toggle sel tagset */
  if (arg->ui & TAGMASK)
    selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
  focus(NULL);
  arrange(selmon);
}

void unfocus(Client *c, int setfocus) {
  // I dont't think we need to do anything here?
}

void focusmon(const Arg *arg) {
  Monitor *m;
  TRACE("");

  if (!mons->next)
    return;
  if ((m = dirtomon(arg->i)) == selmon)
    return;
  unfocus(selmon->sel, 0);
  selmon = m;
  focus(NULL);
  if (mouse_warp)
    warp(selmon->sel);
}

void focusstack(const Arg *arg) {
  TRACE("");
  Client *c = NULL, *i;

  if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen)) {
    TRACEF("no salmon? %p", (void *)selmon->sel);
    return;
  }
  if (arg->i > 0) {
    for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next)
      ;
    if (!c)
      for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next)
        ;
  } else {
    for (i = selmon->clients; i != selmon->sel; i = i->next)
      if (ISVISIBLE(i))
        c = i;
    if (!c)
      for (; i; i = i->next)
        if (ISVISIBLE(i))
          c = i;
  }
  if (c) {
    focus(c);
    restack(selmon);
  } else {
    TRACE("No client to focus");
  }
}

void togglefloating(const Arg *arg) {
  if (!selmon->sel)
    return;
  if (selmon->sel->isfullscreen) /* no support for fullscreen windows */
    return;
  selmon->sel->isfloating = !selmon->sel->isfloating || selmon->sel->isfixed;
  if (selmon->sel->isfloating)
    resize(selmon->sel, selmon->sel->x, selmon->sel->y, selmon->sel->w, selmon->sel->h, 0);
  arrange(selmon);
}

void focus(Client *c) {
  if (!c || !ISVISIBLE(c))
    for (c = selmon->stack; c && !ISVISIBLE(c); c = c->snext)
      ;
  if (selmon->sel && selmon->sel != c)
    unfocus(selmon->sel, 0);
  if (c) {
    if (c->mon != selmon)
      selmon = c->mon;
    if (c->isurgent)
      seturgent(c, 0);
    detachstack(c);
    attachstack(c);
    /* Avoid flickering when another client appears and the border
     * is restored */
    setfocus(c);
  }
  selmon->sel = c;
  arrange(selmon);
}

void setfocus(Client *c) {
  if (!c->neverfocus) {
    SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (void *)1, 0);
    int i;
    for (i = 0; i < 1000; i++) {
      if (SetForegroundWindow(c->hwnd))
        break;
    }
    TRACEF("Focus in %d tries", i);
    SetFocus(c->hwnd);
  }
}

void tilewide(Monitor *m) {
  unsigned int i, n, w, h, mw, mx, ty;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = mx = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
    if (i < m->nmaster) {
      w = (mw - mx) / (MIN(n, m->nmaster) - i);
      resize(c, m->wx + mx, m->wy, w - (2 * c->bw), (m->wh - ty) - (2 * c->bw), 0);
      if (mx + WIDTH(c) < m->ww)
        mx += WIDTH(c);
    } else {
      h = (m->wh - ty) / (n - i);
      resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2 * c->bw), h - (2 * c->bw), 0);
      if (ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c);
    }
}

void monocle(Monitor *m) {
  Client *c;

  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void restack(Monitor *m) {
  if (!m->sel)
    return;
  if (m->sel->isfloating || !m->lt[m->sellt]->arrange) {
    // XRaiseWindow(dpy, m->sel->win);
    SetWindowPos(m->sel->hwnd, HWND_TOP, m->sel->x, m->sel->y, m->sel->w, m->sel->h, SWP_NOACTIVATE);
  }
  if (mouse_warp && m == selmon && (m->tagset[m->seltags] & m->sel->tags) && m->lt[m->sellt]->arrange != &monocle)
    warp(selmon->sel);
}

void detach(Client *c) {
  Client **tc;

  for (tc = &c->mon->clients; *tc && *tc != c; tc = &(*tc)->next)
    ;
  *tc = c->next;
}

void detachstack(Client *c) {
  Client **tc, *t;

  for (tc = &c->mon->stack; *tc && *tc != c; tc = &(*tc)->snext)
    ;
  *tc = c->snext;

  if (c == c->mon->sel) {
    for (t = c->mon->stack; t && !ISVISIBLE(t); t = t->snext)
      ;
    c->mon->sel = t;
  }
}

Monitor *dirtomon(int dir) {
  Monitor *m = NULL;

  if (dir > 0) {
    if (!(m = selmon->next))
      m = mons;
  } else if (selmon == mons)
    for (m = mons; m->next; m = m->next)
      ;
  else
    for (m = mons; m->next != selmon; m = m->next)
      ;
  return m;
}

void unmanage(HWND hwnd) {
  Client *c;
  Monitor *m;
  c = wintoclient(hwnd);
  if (!c)
    return;

  m = c->mon;

  detach(c);
  detachstack(c);
  free(c);
  focus(NULL);
  arrange(m);
}

void manage(HWND hwnd) {
  char name[256];
  if (!IsWindowVisible(hwnd) || IsIconic(hwnd) || !CanMoveWindow(hwnd) || !IsWindowResizableMovable(hwnd) || !GetWindowText(hwnd, name, sizeof(name))) {
    unmanage(hwnd);
    return;
  }
  if (wintoclient(hwnd)) {
    return;
  }

  Client *c;
  RECT rect;
  GetWindowRect(hwnd, &rect);
  Monitor *owner = selmon;
  // the owner of the window is whatever monitor contains the top left corner.
  // If no monitor contains the corner, selmon is used as a fallback
  for (Monitor *m = mons; m; m = m->next) {
    if (rect.left >= m->mx && rect.left < m->mx + m->mw && rect.top >= m->my && rect.top < m->my + m->mh) {
      owner = m;
      break;
    }
  }

  c = ecalloc(1, sizeof(Client));
  c->hwnd = hwnd;
  c->mon = owner;

  c->x = c->oldx = rect.left;
  c->y = c->oldy = rect.top;
  c->w = c->oldw = rect.right - rect.left;
  c->h = c->oldh = rect.bottom - rect.top;

  if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
    c->x = c->mon->wx + c->mon->ww - WIDTH(c);
  if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
    c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
  c->x = MAX(c->x, c->mon->wx);
  c->y = MAX(c->y, c->mon->wy);
  c->bw = borderpx;

  strncpy_s(c->name, sizeof(c->name) - 1, name, sizeof(c->name) - 1);
  owner->sel = c;
  applyrules(c);

  attachtop(c);
  attachstack(c);
  c->mon->sel = c;
  arrange(c->mon);
  focus(NULL);
}

void setupmons(void) {
  // enumerate all connected monitors and get their resoluti|n
  DISPLAY_DEVICE dd = {0};
  dd.cb = sizeof(dd);

  for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
    DEVMODE dm;
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettings(dd.DeviceName, ENUM_CURRENT_SETTINGS, &dm)) {
      Monitor *m = createmon();
      m->dd = dd;
      m->mw = m->ww = dm.dmPelsWidth;
      m->mh = m->wh = dm.dmPelsHeight;
      m->mx = m->wx = dm.dmPosition.x;
      m->my = m->wy = dm.dmPosition.y;
      m->wh -= barheight;

      m->next = mons;
      mons = m;
    }
  }
  selmon = mons;
  describe_monitors();
}

void setupclients(void) {
  HWND hwnd = GetTopWindow(NULL);
  while (hwnd) {
    if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && CanMoveWindow(hwnd) && IsWindowResizableMovable(hwnd)) {
      manage(hwnd);
    }
    hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
  }
}

void setnmaster(const Arg *arg) { selmon->nmaster = MAX(arg->i, 0); }

void incnmaster(const Arg *arg) {
  selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
  arrange(selmon);
}

void setlayout(const Arg *arg) {
  if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
    selmon->sellt ^= 1;
  if (arg && arg->v)
    selmon->lt[selmon->sellt] = (Layout *)arg->v;
  if (selmon->sel)
    arrange(selmon);
}

void killclient(const Arg *arg) {
  if (!selmon->sel)
    return;
  if (!IsWindowVisible(selmon->sel->hwnd))
    return;
  PostMessage(selmon->sel->hwnd, WM_CLOSE, 0, 0);
}

void zoom(const Arg *arg) {
  Client *c = selmon->sel;

  if (!selmon->lt[selmon->sellt]->arrange || selmon->sel == NULL)
    return;
  if (c == nexttiled(selmon->clients)) {
    if (!c || !(c = nexttiled(c->next)))
      return;
  }
  pop(c);
}

/* arg > 1.0 will set mfact absolutely */
void setmfact(const Arg *arg) {
  float f;

  if (!arg || !selmon->lt[selmon->sellt]->arrange)
    return;
  f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
  if (f < 0.05 || f > 0.95)
    return;
  selmon->mfact = f;
  arrange(selmon);
}

int GetTaskbarHeight(void) {
  RECT taskbarRect;
  SystemParametersInfo(SPI_GETWORKAREA, 0, &taskbarRect, 0);
  return GetSystemMetrics(SM_CYSCREEN) - (taskbarRect.bottom - taskbarRect.top);
}

void scan(void) {
  barheight = GetTaskbarHeight();
  setupmons();
  setupclients();
}

void sendmon(Client *c, Monitor *m) {
  if (c->mon == m)
    return;
  unfocus(c, 1);
  detach(c);
  detachstack(c);
  c->mon = m;
  c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
  attachtop(c);
  attachstack(c);
  focus(NULL);
  arrange(NULL);
}

Client *wintoclient(HWND hwnd) {
  Client *c;
  Monitor *m;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->hwnd == hwnd)
        return c;
  return NULL;
}

Monitor *wintomon(HWND hwnd) {
  Monitor *m;
  Client *c;

  for (m = mons; m; m = m->next)
    for (c = m->clients; c; c = c->next)
      if (c->hwnd == hwnd)
        return m;
  return NULL;
}

// crash handler
LONG WINAPI crash_handler(struct _EXCEPTION_POINTERS *ExceptionInfo) {
  die("Crash");
  exit(1);
}

void install_crash_handler(void) {
  SetErrorMode(SEM_NOGPFAULTERRORBOX);
  SetUnhandledExceptionFilter(crash_handler);
}

void pushdown(const Arg *arg) {
  Client *sel = selmon->sel, *c;

  if (!sel || sel->isfloating)
    return;
  if ((c = nexttiled(sel->next))) {
    detach(sel);
    sel->next = c->next;
    c->next = sel;
  }
  focus(sel);
  arrange(selmon);
}

void pushup(const Arg *arg) {
  Client *sel = selmon->sel, *c;

  if (!sel || sel->isfloating)
    return;
  if ((c = prevtiled(sel))) {
    if (c == nexttiled(selmon->clients)) {
      zoom(arg);
    } else {
      detach(sel);
      sel->next = c;
      for (c = selmon->clients; c->next != sel->next; c = c->next)
        ;
      c->next = sel;
    }
  }
  focus(sel);
  arrange(selmon);
}

void quit(const Arg *arg) {
  running = 0;
  // we need to trigger an event so the main loop will stop hanging in GetMessage
  PostMessage(NULL, WM_QUIT, 0, 0);
}

void ensurefocused(void) {
  HWND hwnd = GetForegroundWindow();
  if (hwnd == NULL)
    return;
  if (selmon && selmon->sel && selmon->sel->hwnd != hwnd) {
    Client *c = wintoclient(hwnd);
    if (c == NULL)
      manage(hwnd);
    c = wintoclient(hwnd);
    if (c == NULL)
      return;
    focus(c);
  }
}

void CALLBACK WindowCallback(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
  switch (event) {
  case EVENT_OBJECT_CREATE:
  case EVENT_OBJECT_SHOW:
  case EVENT_OBJECT_FOCUS:
  case EVENT_SYSTEM_MINIMIZEEND:
    manage(hwnd);
    break;
  case EVENT_OBJECT_DESTROY:
  case EVENT_OBJECT_HIDE:
  case EVENT_SYSTEM_MINIMIZESTART:
    unmanage(hwnd);
    break;
  }
  // just in case focus was changed in some other way I don't know about
  ensurefocused();
}
int main(int argc, char **argv) {
  install_crash_handler();
  // check if wdwm.exe is already running.
  HANDLE h = CreateMutex(NULL, TRUE, "wdwm");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    die("wdwm is already running");
  }

  HINSTANCE hInstance = GetModuleHandle(NULL);
  HHOOK keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
  if (keyboardHook == NULL)
    die("Failed to register keyboard hook:");
  HHOOK mouseHook = SetWindowsHookExA(WH_MOUSE_LL, MouseProc, hInstance, 0);
  if (mouseHook == NULL)
    die("Failed to register mouse hook:");

  HWINEVENTHOOK eventHook = SetWinEventHook(EVENT_MIN, EVENT_MAX, NULL, WindowCallback, 0, 0, WINEVENT_OUTOFCONTEXT);
  if (eventHook == NULL)
    die("Failed to register window event hook:");

  scan();

  MSG msg;
  while (running) {
    if (GetMessage(&msg, NULL, 0, 0) > 0) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  UnhookWindowsHookEx(keyboardHook);
  UnhookWindowsHookEx(mouseHook);
  UnhookWinEvent(eventHook);
  CloseHandle(h);

  return 0;
}
