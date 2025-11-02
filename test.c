
#include <windows.h>
#include <fileapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>

int main() {
  char *path = ".";
  printf("watching %s for changes...\n", path);

  HANDLE file = CreateFile(path,
    FILE_LIST_DIRECTORY,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
    NULL);
  if(file == INVALID_HANDLE_VALUE){
    printf("CreateFile error: %08lX", GetLastError());
    return 1;
  }
  OVERLAPPED overlapped;
  overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);

  uint8_t change_buf[1024];
  BOOL success = ReadDirectoryChangesW(
    file, change_buf, 1024, TRUE,
    FILE_NOTIFY_CHANGE_FILE_NAME  |
    FILE_NOTIFY_CHANGE_DIR_NAME   |
    FILE_NOTIFY_CHANGE_LAST_WRITE,
    NULL, &overlapped, NULL);

  while (true) {
    DWORD result = WaitForSingleObject(overlapped.hEvent, 0);

    if (result == WAIT_OBJECT_0) {
      DWORD bytes_transferred;
      GetOverlappedResult(file, &overlapped, &bytes_transferred, FALSE);

      FILE_NOTIFY_INFORMATION *event = (FILE_NOTIFY_INFORMATION*)change_buf;

      for (;;) {
        DWORD name_len = event->FileNameLength / sizeof(wchar_t);
        for(DWORD i = 0; i < event->FileNameLength; ++i){
          printf("%02X ", ((char*)event->FileName)[i]);
        }
        printf("\n");
        for(DWORD i = 0; i < name_len; ++i){
          printf("%c", event->FileName[i]);
        }
        printf("\n");

        switch (event->Action) {
          case FILE_ACTION_ADDED: {
            wprintf(L"       Added: %.*s\n", name_len, event->FileName);
          } break;

          case FILE_ACTION_REMOVED: {
            wprintf(L"     Removed: %.*s\n", name_len, event->FileName);
          } break;

          case FILE_ACTION_MODIFIED: {
              wprintf(L"    Modified: %.*s\n", name_len, event->FileName);
          } break;

          case FILE_ACTION_RENAMED_OLD_NAME: {
            wprintf(L"Renamed from: %.*s\n", name_len, event->FileName);
          } break;

          case FILE_ACTION_RENAMED_NEW_NAME: {
            wprintf(L"          to: %.*s\n", name_len, event->FileName);
          } break;

          default: {
            printf("Unknown action!\n");
          } break;
        }

        // Are there more events to handle?
        if (event->NextEntryOffset) {
          *((uint8_t**)&event) += event->NextEntryOffset;
        } else {
          break;
        }
      }
      
      // Queue the next event
      BOOL success = ReadDirectoryChangesW(
            file, change_buf, 1024, TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME  |
            FILE_NOTIFY_CHANGE_DIR_NAME   |
            FILE_NOTIFY_CHANGE_LAST_WRITE,
            NULL, &overlapped, NULL);

    }

    // Do other loop stuff here...
  }
}
