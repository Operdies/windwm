#pragma once

#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#include <winuser.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


/* macros */
#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define INTERSECT(x, y, w, h, m)                                                                                       \
  (MAX(0, MIN((x) + (w), (m)->wx + (m)->ww) - MAX((x), (m)->wx)) *                                                     \
   MAX(0, MIN((y) + (h), (m)->wy + (m)->wh) - MAX((y), (m)->wy)))
#define ISVISIBLEONTAG(C, T) ((C->tags & T))
#define ISVISIBLE(C) ISVISIBLEONTAG(C, C->mon->tagset[C->mon->seltags])
#define LENGTH(X) (sizeof X / sizeof X[0])
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(X) ((X)->w + 2 * (X)->bw)
#define HEIGHT(X) ((X)->h + 2 * (X)->bw)
#define TAGMASK ((1 << LENGTH(tags)) - 1)

#define ShiftMask (1 << 0)
#define MetaMask (1 << 1)
#define SuperMask (1 << 2)
#define ControlMask (1 << 3)

/* types */

typedef unsigned char u8;
typedef unsigned long long u64;

typedef union {
  int i;
  unsigned int ui;
  float f;
  const void *v;
} Arg;

typedef struct {
  u8 mods;
  u8 vk;
  void (*func)(const Arg *);
  const Arg arg;
} Key;

typedef struct {
  bool elevate;
  const char *cmd;
  const char *args;
  const char *wd;
} SpawnArgs;

typedef struct Monitor Monitor;
typedef struct Client Client;

struct Client {
  char name[256];
  float mina, maxa;
  int x, y, w, h;
  int oldx, oldy, oldw, oldh;
  int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
  double dpix, dpiy;
  int bw, oldbw;
  unsigned int tags;
  int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
  Client *next;
  Client *snext;
  Monitor *mon;
  HWND hwnd;
};

typedef struct {
  const char *symbol;
  void (*arrange)(Monitor *);
} Layout;

struct Monitor {
  char name[100];
  float mfact;
  int nmaster;
  int num;
  int mx, my, mw, mh; /* screen size */
  int wx, wy, ww, wh; /* window area  */
  unsigned int seltags;
  unsigned int sellt;
  unsigned int tagset[2];
  int showbar;
  int topbar;
  Client *clients;
  Client *sel;
  Client *stack;
  Monitor *next;
  const Layout *lt[2];
};

/* functions */

Client *wintoclient(HWND hwnd);

void applyrules(Client *c);
// int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
void arrange(Monitor *m);
void arrangemon(Monitor *m);
void attach(Client *c);
void attachtop(Client *c);
void attachstack(Client *c);
void checkotherwm(void);
void cleanup(void);
void cleanupmon(Monitor *mon);
void configure(Client *c);
Monitor *createmon(void);
void detach(Client *c);
void detachstack(Client *c);
Monitor *dirtomon(int dir);
void focus(Client *c);
void focusmon(const Arg *arg);
void focusstack(const Arg *arg);
void focusstack_multimon(const Arg *arg);
Monitor *prevmon(Monitor *m);
Monitor *nextmon(Monitor *m);
Client *prevclient(Client *clients, Client *sel);
Client *nextclient(Client *clients, Client *sel);
int getrootptr(int *x, int *y);
void grabbuttons(Client *c, int focused);
void grabkeys(void);
void setnmaster(const Arg *arg);
void incnmaster(const Arg *arg);
void killclient(const Arg *arg);
void manage(HWND hwnd, Monitor *owner);
void monocle(Monitor *m);
void movemouse(const Arg *arg);
Client *nexttiled(Client *c);
void pop(Client *c);
Client *prevtiled(Client *c);
void pushdown(const Arg *arg);
void pushup(const Arg *arg);
void quit(const Arg *arg);
Monitor *recttomon(int x, int y, int w, int h);
void resize(Client *c, int x, int y, int w, int h, int interact);
void resizeclient(Client *c, int x, int y, int w, int h, UINT flags);
void setfloating(Client *c, bool f);
void resizemouse(const Arg *arg);
void restack(Monitor *m);
void run(void);
void scan(void);
void sendmon(Client *c, Monitor *m);
void setclientstate(Client *c, long state);
void setfocus(Client *c);
void setfullscreen(Client *c, int fullscreen);
void setlayout(const Arg *arg);
void setmfact(const Arg *arg);
void setup(void);
void seturgent(Client *c, int urg);
void showhide(Client *c);
int solitary(Client *c);
void spawn(const Arg *a);
void splitmon(const Arg *arg);
void tag(const Arg *arg);
void tagmon(const Arg *arg);
void tilewide(Monitor *m);
void tilewhat(Monitor *m);
void togglebar(const Arg *arg);
void togglebarelems(const Arg *arg);
void togglefloating(const Arg *arg);
void toggletag(const Arg *arg);
void toggleview(const Arg *arg);
void unfocus(Client *c, int setfocus);
void unmanage(HWND hwnd, const char *reason);
void updatebarpos(Monitor *m);
void updatebars(void);
void updateclientlist(void);
int updategeom(void);
void updatenumlockmask(void);
void updatesizehints(Client *c);
void updatestatus(void);
void updatetitle(Client *c);
void updatewindowtype(Client *c);
void updatewmhints(Client *c);
void view(const Arg *arg);
void warp(const Client *c);
Client *wintoclient(HWND w);
Monitor *wintomon(HWND w);
void zoom(const Arg *arg);

/* globals */
Monitor *mons = NULL, *selmon = NULL;

bool running = 1;
