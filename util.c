#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"

void errormsg(const char *fmt, ...) {
  va_list ap;
  HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO ci;
  WORD sa;

  GetConsoleScreenBufferInfo(h, &ci);
  sa = ci.wAttributes;
  SetConsoleTextAttribute(h, FOREGROUND_RED);

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    fputc(' ', stderr);
    wchar_t buf[256];

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                   (sizeof(buf) / sizeof(wchar_t)), NULL);
    fwprintf(stderr, L"%ws", buf);
  } else {
    fputc('\n', stderr);
  }
  SetConsoleTextAttribute(h, sa);
}

void die(const char *fmt, ...) {
  va_list ap;
  HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO ci;
  WORD sa;

  GetConsoleScreenBufferInfo(h, &ci);
  sa = ci.wAttributes;
  SetConsoleTextAttribute(h, FOREGROUND_RED);

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
    fputc(' ', stderr);
    wchar_t buf[256];

    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                   (sizeof(buf) / sizeof(wchar_t)), NULL);
    fwprintf(stderr, L"%ws", buf);
  } else {
    fputc('\n', stderr);
  }
  SetConsoleTextAttribute(h, sa);

  exit(1);
}

void *ecalloc(size_t nmemb, size_t size) {
  void *p;

  if (!(p = calloc(nmemb, size)))
    die("calloc:");
  return p;
}
