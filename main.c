#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <stdio.h>
#include <windows.h>


void print_all_window_titles(void) {
  // enumerate all windows and print their titles
  HWND hwnd = GetTopWindow(NULL);
  char title[256] = {0};
  while (hwnd) {
    if (IsWindowVisible(hwnd) && GetWindowText(hwnd, title, sizeof(title))) {
      printf("Title: %s\n", title);
      // check if window is minimized
      if (IsIconic(hwnd)) {
        printf("\tWindow is minimized\n");
      } else {
        // check window dimensions
        RECT rect;
        GetWindowRect(hwnd, &rect);
        printf("\tWindow dimensions: %ld x %ld\n", rect.right - rect.left,
               rect.bottom - rect.top);
      }
    }
    hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
  }
}

int main(int argc, char **argv) {
  printf("Greetings\n");
  print_all_window_titles();
  return 0;
}
