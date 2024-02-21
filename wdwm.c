#include "wdwm.h"
#include "config.h"
#include "util.h"
#include <Dwmapi.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <string.h>
#include <winuser.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "Uxtheme.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")

void setwindowpos(Client *c, int x, int y, int w, int h, UINT flags) {
  // if (c->x == x && c->y == y && c->w == w && c->h == h)
  //   return;
  // TRACEF("Update: (%d,%d) %dx%d", x, y, w, h);
  // flags |= SWP_NOACTIVATE;
  // flags |= SWP_ASYNCWINDOWPOS;

  if (c->isfloating) {
    // preserve current position but apply flags and make topmost
    SetWindowPos(c->hwnd, HWND_TOP, x, y, w, h, flags);
  } else {
    RECT before, after;
    GetWindowRect(c->hwnd, &before);
    SetWindowPos(c->hwnd, HWND_NOTOPMOST, x, y, w, h, flags);
    GetWindowRect(c->hwnd, &after);
    c->x = after.left;
    c->y = after.top;
    c->w = after.right - after.left;
    c->h = after.bottom - after.top;

    // TRACEF("Before: (%ld,%ld) %ldx%ld", before.left, before.top, before.right - before.left, before.bottom - before.top);
    // TRACEF(" After: (%ld,%ld) %ldx%ld", after.left, after.top, after.right - after.left, after.bottom - after.top);
    // TRACEF("Client: (%d,%d) %dx%d", c->x, c->y, c->w, c->h);
  }
}

void UpdateForegroundLockTimeout(DWORD timeoutMs) {
  DWORD currentValue;
  if (!SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &currentValue, 0)) {
    errormsg("Failed to read current foreground lock timeout:");
    return;
  }

  if (currentValue == timeoutMs)
    return;

  TRACEF("Trying to update foreground lock timeout: %lu -> %lu", currentValue, timeoutMs);
  if (!SystemParametersInfo(SPI_SETFOREGROUNDLOCKTIMEOUT, 0, (void*)(u64)timeoutMs, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    errormsg("Setting foreground lock timeout failed:");
    return;
  }

  DWORD newvalue;
  if (SystemParametersInfo(SPI_GETFOREGROUNDLOCKTIMEOUT, 0, &newvalue, 0))
    TRACEF("Updated foreground lock timeout: %lu -> %lu", currentValue, newvalue);
  else
    errormsg("Unable to retrieve updated value:");
}

void _spawn(const char *cmd, bool elevate) {
  if (!ShellExecuteA(NULL, elevate ? "runas" : "open", cmd, 0, 0, SW_SHOWNORMAL))
    errormsg("Failed to spawn %s%s:", elevate ? "elevated " : "", cmd);
}
void spawn(const Arg *arg) { _spawn(arg->v, false); }
void spawn_elevated(const Arg *arg) { _spawn(arg->v, true); }

#define WIN_DOWN (keystate[VK_LWIN] || keystate[VK_RWIN])
#define ALT_DOWN (keystate[VK_LMENU] || keystate[VK_RMENU] || keystate[VK_MENU])
#define CTRL_DOWN (keystate[VK_LCONTROL] || keystate[VK_RCONTROL] || keystate[VK_CONTROL])
#define SHIFT_DOWN (keystate[VK_LSHIFT] || keystate[VK_RSHIFT] || keystate[VK_SHIFT])
#define MODMAP (u8)((WIN_DOWN ? SuperMask : 0) | (ALT_DOWN ? MetaMask : 0) | (CTRL_DOWN ? ControlMask : 0) | (SHIFT_DOWN ? ShiftMask : 0))

static bool keystate[0xff] = {0};
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
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
        if (is_release)
          k->func(&k->arg);
      }
    }
  }
  return intercept || CallNextHookEx(NULL, nCode, wParam, lParam);
}

static bool mouseenabled = true;
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
  static int lbutton = 0;
  static int rbutton = 0;
  static POINT dragstart;
  static POINT clientstart;
  static Client *dragged = NULL;
  Client *c;
  HWND hwnd;

  if (nCode == HC_ACTION) {
    switch (wParam) {
    case WM_LBUTTONDOWN:
      lbutton = 1;
      break;
    case WM_LBUTTONUP:
      lbutton = 0;
      break;
    case WM_RBUTTONDOWN:
      rbutton = 1;
      break;
    case WM_RBUTTONUP:
      rbutton = 0;
      break;
    }
    if (wParam == WM_MOUSEMOVE) {
      PMSLLHOOKSTRUCT p = (PMSLLHOOKSTRUCT)lParam;
      // clang-format off
      for (hwnd = WindowFromPoint(p->pt), c = wintoclient(hwnd); 
           hwnd && !c; 
           hwnd = GetParent(hwnd), c = wintoclient(hwnd));
      // clang-format on
      if (hwnd) {
        // drag move
        if (mouse_drag && lbutton && ALT_DOWN) {
          // start drag
          if (!dragged) {
            TRACE("Start drag");
            dragstart = p->pt;
            dragged = wintoclient(hwnd);
            if (dragged) {
              clientstart.x = dragged->x;
              clientstart.y = dragged->y;
              mouseenabled = false;
              dragged->isfloating = true;
            }
          }
          if (dragged) {
            int deltax = p->pt.x - dragstart.x;
            int deltay = p->pt.y - dragstart.y;

            TRACEF("Drag: %d, %d", deltax, deltay);

            SetCursorPos(p->pt.x, p->pt.y);
            setwindowpos(dragged, clientstart.x + deltax, clientstart.y + deltay, dragged->w, dragged->h, 0);
            return 1;
          }
          // drag resize
        } else if (mouse_resize && rbutton && ALT_DOWN) {
          // focus mouseover
        } else if (mouse_focus && mouseenabled) {
          if (c && selmon->sel != c) {
            bool tmp = mouse_warp;
            mouse_warp = false;
            focus(c);
            mouse_warp = tmp;
          }
        }
      }
    }
    dragged = NULL;
    mouseenabled = true;
  }
  return CallNextHookEx(NULL, nCode, wParam, lParam);
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

  // Check if window is visible and not minimized
  if ((style & WS_VISIBLE) && !(style & WS_MINIMIZE)) {
    // Check if window is sizable or has a border
    // Check if window is not a child, popup, or dialog
    if (!(style & WS_CHILD) && !(style & WS_POPUP)) {
      return true;
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
  setwindowpos(c, c->x, c->y, c->w, c->h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void warpmouse(int x, int y) { SetCursorPos(x, y); }

void warp(const Client *c) {
  POINT p;
  if (c) {
    p.x = c->x + c->w / 2;
    p.y = c->y + c->h / 2;
  } else {
    p.x = selmon->mx + selmon->mw / 2;
    p.y = selmon->my + selmon->mh / 2;
  }
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

void showhide(Client *c) {
  if (!c)
    return;
  if (!IsWindow(c->hwnd)) {
    unmanage(c->hwnd);
    return;
  }

  // Setting the window placement appears to be needed in order for restore / minimize to happen without animations
  WINDOWPLACEMENT wndpl = {
      .length = sizeof(WINDOWPLACEMENT),
      .ptMinPosition = (POINT){.x = c->x, .y = c->y},
      .ptMaxPosition = (POINT){.x = c->x, .y = c->y},
      .rcNormalPosition =
          (RECT){
              .left = c->x,
              .top = c->y,
              .right = c->x + c->w,
              .bottom = c->y + c->h,
          },
  };
  if (ISVISIBLE(c)) {
    // TRACEF("Showing client: %s", c->name);
    /* show clients top down */
    wndpl.showCmd = SW_RESTORE;
    if (!SetWindowPlacement(c->hwnd, &wndpl))
      errormsg("Error restoring window:");
    setwindowpos(c, c->x, c->y, c->w, c->h, SWP_NOACTIVATE);
    if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
      resize(c, c->x, c->y, c->w, c->h, 0);
    showhide(c->snext);
  } else {
    // TRACEF("Hiding client: %s", c->name);
    /* hide clients bottom up */
    showhide(c->snext);
    wndpl.showCmd = SW_MINIMIZE;
    if (!SetWindowPlacement(c->hwnd, &wndpl))
      errormsg("Error minimizing window:");
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
  Client *c = NULL, *i;

  if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen)) {
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
    // TRACEF("Focus %s", c->name);
  }
  setfocus(c);
  selmon->sel = c;
  arrange(selmon);
}

int forceforeground(HWND hwnd, int n) {
  for (int i = 0; i < n; i++) {
    if (SetForegroundWindow(hwnd))
      return true;
  }
  return false;
}

void setfocus(Client *c) {
  if (c && !c->neverfocus) {
    if (!forceforeground(c->hwnd, 300)) {
      UpdateForegroundLockTimeout(0);
      if (!forceforeground(c->hwnd, 300)) {
        errormsg("Foreground %s failed", c->name);
      }
    }
    SetFocus(c->hwnd);
  }
}

void tilewide(Monitor *m) {
  unsigned int i, n, w, h, mw, mx, ty;
  int gap;
  Client *c;

  for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++)
    ;
  if (n == 0)
    return;

  gap = n > 1 ? gaps : 0;

  if (n > m->nmaster)
    mw = m->nmaster ? m->ww * m->mfact : 0;
  else
    mw = m->ww;
  for (i = mx = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++) {
    int x, y, w2, h2;
    if (i < m->nmaster) {
      w = (mw - mx) / (MIN(n, m->nmaster) - i);
      x = m->wx + mx;
      y = m->wy;
      w2 = w - (2 * c->bw);
      h2 = (m->wh - ty) - (2 * c->bw);
      resize(c, x + gap, y + gap, w2 - (gap), h2 - (gap * 2), 0);
      if (mx + WIDTH(c) < m->ww)
        mx += WIDTH(c) + gap;
    } else {
      h = (m->wh - ty) / (n - i);
      x = m->wx + mw;
      y = m->wy + ty;
      w2 = m->ww - mw - (2 * c->bw);
      h2 = h - (2 * c->bw);
      resize(c, x + gap, y + gap, w2 - (gap * 2), h2 - (gap * 2), 0);
      if (ty + HEIGHT(c) < m->wh)
        ty += HEIGHT(c) + gap;
    }
  }
}

void monocle(Monitor *m) {
  Client *c;

  for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
    resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
}

void restack(Monitor *m) {
  if (m && m->sel) {
    if (m->sel->isfloating || !m->lt[m->sellt]->arrange) {
      // XRaiseWindow(dpy, m->sel->win);
      setwindowpos(m->sel, m->sel->x, m->sel->y, m->sel->w, m->sel->h, SWP_NOACTIVATE);
    }
    if (mouse_warp && m == selmon && (m->tagset[m->seltags] & m->sel->tags) && m->lt[m->sellt]->arrange != &monocle)
      warp(selmon->sel);
  }
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

  TRACEF("Unmanage %s", c->name);

  m = c->mon;

  detach(c);
  detachstack(c);
  free(c);
  focus(NULL);
  arrange(m);
}

void manage(HWND hwnd, Monitor *owner) {
  char name[256];
  if (wintoclient(hwnd)) {
    return;
  }
  if (!CanMoveWindow(hwnd) || !IsWindowResizableMovable(hwnd) || !GetWindowText(hwnd, name, sizeof(name))) {
    unmanage(hwnd);
    return;
  }

  TRACEF("Manage %s", name);

  int corner = DWMWCP_DONOTROUND;
  if (!DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner)))
    errormsg("Failed to set corner preferences for %s", name);

  LONG lStyle = GetWindowLong(hwnd, GWL_STYLE);
  lStyle &= ~(WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU);
  SetWindowLong(hwnd, GWL_STYLE, lStyle);

  LONG lExStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  // lExStyle |= WS_EX_CLIENTEDGE;
  // lExStyle |= WS_EX_STATICEDGE;
  lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
  SetWindowLong(hwnd, GWL_EXSTYLE, lExStyle);
  SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

  Client *c;
  RECT rect;
  GetWindowRect(hwnd, &rect);
  if (!owner) {
    // the owner of the window is whatever monitor contains the top left corner.
    // If no monitor contains the corner, selmon is used as a fallback
    for (Monitor *m = mons; m; m = m->next) {
      if (rect.left >= m->mx && rect.left < m->mx + m->mw && rect.top >= m->my && rect.top < m->my + m->mh) {
        owner = m;
        break;
      }
    }
    if (!owner)
      owner = selmon;
  }

  c = ecalloc(1, sizeof(Client));
  c->hwnd = hwnd;
  c->mon = owner;
  c->dpix = 1;
  c->dpiy = 1;

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
      HMONITOR hmon = MonitorFromPoint((POINT){dm.dmPosition.x, dm.dmPosition.y}, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi;
      mi.cbSize = sizeof(mi);
      GetMonitorInfo(hmon, &mi);

      m->dd = dd;
      m->hmon = hmon;
      m->mi = mi;
      m->mw = mi.rcMonitor.right - mi.rcMonitor.left;
      m->mh = mi.rcMonitor.bottom - mi.rcMonitor.top;
      m->mx = mi.rcMonitor.left;
      m->my = mi.rcMonitor.top;
      m->ww = mi.rcWork.right - mi.rcWork.left;
      m->wh = mi.rcWork.bottom - mi.rcWork.top;
      m->wx = mi.rcWork.left;
      m->wy = mi.rcWork.top;

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
    if (CanMoveWindow(hwnd) && IsWindowResizableMovable(hwnd)) {
      manage(hwnd, NULL);
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

  if (hwnd)
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
  if (hwnd != NULL) {
    if (selmon && selmon->sel && selmon->sel->hwnd != hwnd) {
      Client *c = wintoclient(hwnd);
      if (c == NULL)
        manage(hwnd, selmon);
      c = wintoclient(hwnd);
      if (c != NULL)
        focus(c);
    }
  }
}

bool unmanaged_matches(const unmanaged_t *rule, const char *name) {
  int n = strlen(name);
  switch (rule->matchtype) {
  case MATCHES:
    return strncmp(rule->title, name, n) == 0;
  case STARTSWITH:
    return strncmp(rule->title, name, strlen(rule->title)) == 0;
  case CONTAINS:
    return strstr(name, rule->title) != NULL;
  case ENDSWITH: {
    int lenstr = strlen(name);
    int lensuffix = strlen(rule->title);
    if (lensuffix > lenstr)
      return false;
    return strncmp(name + lenstr - lensuffix, rule->title, lensuffix) == 0;
  }
  }
  return false;
}

void CALLBACK WindowProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime) {
  char name[256] = {0};
  if (GetWindowText(hwnd, name, sizeof(name))) {
    for (int i = 0; i < LENGTH(unmanaged); i++) {
      if (unmanaged_matches(&unmanaged[i], name))
        return;
    }
  }

  switch (event) {
  case EVENT_OBJECT_CREATE:
  case EVENT_OBJECT_SHOW:
  case EVENT_OBJECT_FOCUS:
  case EVENT_SYSTEM_MINIMIZEEND:
    manage(hwnd, selmon);
    break;
  case EVENT_OBJECT_DESTROY:
    // case EVENT_OBJECT_HIDE: Windows are being hidden by showhide: ShowWindow(SW_HIDE)
    // case EVENT_SYSTEM_MINIMIZESTART:
    unmanage(hwnd);
    break;
  default:
    // TRACEF("Unhandled event: %ld", event);
    break;
  }
  Client *c = wintoclient(GetForegroundWindow());
  if (c) {
    selmon = c->mon;
    selmon->sel = c;
  }
  // just in case focus was changed in some other way I don't know about
  // ensurefocused();
}

void scan(void) {
  UpdateForegroundLockTimeout(0);
  HWND fg = GetForegroundWindow();
  setupmons();
  setupclients();

  Client *c = wintoclient(fg);
  focus(c);
  TRACE("Setup complete.");
}

int main(int argc, char **argv) {
  HHOOK keyboardHook, mouseHook;
  keyboardHook = mouseHook = NULL;
  install_crash_handler();
  // check if wdwm.exe is already running.
  HANDLE h = CreateMutexA(NULL, TRUE, "wdwm");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    die("wdwm is already running");
  }

  HINSTANCE hInstance = GetModuleHandle(NULL);
  keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
  if (keyboardHook == NULL)
    die("Failed to register keyboard hook:");
  if (mouse_focus || mouse_drag || mouse_resize) {
    mouseHook = SetWindowsHookExA(WH_MOUSE_LL, MouseProc, hInstance, 0);
    if (mouseHook == NULL)
      die("Failed to register mouse hook:");
  }

  HWINEVENTHOOK eventHook = SetWinEventHook(EVENT_MIN, EVENT_MAX, NULL, WindowProc, 0, 0, WINEVENT_OUTOFCONTEXT);
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

  if (keyboardHook)
    UnhookWindowsHookEx(keyboardHook);
  if (mouseHook)
    UnhookWindowsHookEx(mouseHook);
  if (eventHook)
    UnhookWinEvent(eventHook);
  CloseHandle(h);

  return 0;
}
