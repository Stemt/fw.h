#ifndef FW_H_
#define FW_H_
#include <string.h>
#include <stdbool.h>

#if defined __linux
#include <sys/inotify.h>
#include <linux/limits.h>
#include <unistd.h>
#include <errno.h>
#define FW_NAME_MAX NAME_MAX
#elif defined __WIN32
#include <windows.h>
#include <fileapi.h>
#include <shlwapi.h>
#define FW_NAME_MAX MAX_PATH 
#else
#error "Platform not supported"
#endif


typedef enum{
#if defined __linux
  FW_CREATE     = IN_CREATE,
  FW_DELETE     = IN_DELETE,
  FW_MODIFY     = IN_MODIFY,
  FW_MOVED_FROM = IN_MOVED_FROM,
  FW_MOVED_TO   = IN_MOVED_TO,
#elif defined __WIN32
  FW_CREATE     = (1<<0),
  FW_DELETE     = (1<<1),
  FW_MODIFY     = (1<<2),
  FW_MOVED_FROM = (1<<3),
  FW_MOVED_TO   = (1<<4),
#endif
} FW_Event;

typedef enum{
  FW_E_UNKNOWN,
  FW_E_INVALID_ARGUMENT,
  FW_E_PATH_NOT_FOUND,
  FW_E_PATH_TOO_LONG,
  FW_E_PLATFORM_LIMIT,
  FW_E_ACCESS_DENIED,
  FW_E_BAD_STATE,
  FW_E_NO_EVENT,
  FW_E_IO_ERROR,
} FW_Error;

typedef struct{
  FW_Error error;
  FW_Event watch_events;
  FW_Event received_events;
  char name[FW_NAME_MAX+1];

#if defined __linux
  int fd;
  int wd;

#elif defined __WIN32
  HANDLE handle;
  char change_buffer[1024];
  FILE_NOTIFY_INFORMATION* next_event;
  OVERLAPPED event_info;
#endif
  
} FW;

// --- polling fucntions ---
bool fw_init(FW* self, const char* path, FW_Event events);
bool fw_watch(FW* self);
void fw_deinit(FW* self);
bool fw_once(FW* self, const char* path, FW_Event events);

// --- event data getters ---
FW_Event fw_event(FW* self);
const char* fw_name(FW* self);

// --- error handling ---
const char* fw_strerror(FW_Error error);
FW_Error fw_error(FW* self);

#ifdef FW_IMPLEMENTATION
bool fw_init(FW* self, const char* path, FW_Event events){
  self->watch_events = events;

#if defined __linux

  self->fd = inotify_init();

  if(self->fd < 0){
    switch(errno){
      case EINVAL: self->error = FW_E_INVALID_ARGUMENT; break;
      case ENOMEM: self->error = FW_E_PLATFORM_LIMIT; break;
      case EMFILE: self->error = FW_E_PLATFORM_LIMIT; break;
      default: self->error = FW_E_UNKNOWN; break;
    }
    return false;
  }

  self->wd = inotify_add_watch(self->fd, path, events);
  if(self->wd < 0){
    switch(errno){
      case EACCES: self->error = FW_E_ACCESS_DENIED; break;
      case EEXIST: break; // ignore if file already watched (should not be possible anyway)
      case EFAULT: self->error = FW_E_PATH_NOT_FOUND; break;
      case ENAMETOOLONG: self->error = FW_E_PATH_TOO_LONG; break;
      case ENOENT: self->error = FW_E_PATH_NOT_FOUND; break;
      case EBADF:  self->error = FW_E_UNKNOWN; break;
      case EINVAL: self->error = FW_E_INVALID_ARGUMENT; break;
      case ENOMEM: self->error = FW_E_PLATFORM_LIMIT; break;
      case EMFILE: self->error = FW_E_PLATFORM_LIMIT; break;
      case ENOSPC: self->error = FW_E_PLATFORM_LIMIT; break;
      case ENOTDIR: self->error = FW_E_INVALID_ARGUMENT; break;
      default: self->error = FW_E_UNKNOWN; break;
    }
    return false;
  }
  return true;

#elif defined __WIN32

  self->next_event = NULL;
  self->handle = CreateFile(strdup(path),
      FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      NULL,
      OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
      NULL);

  if(self->handle == INVALID_HANDLE_VALUE){
    self->error = FW_E_UNKNOWN;
    return false;
  }

  self->event_info.hEvent = CreateEvent(NULL, FALSE, 0, NULL);
  if(self->event_info.hEvent == INVALID_HANDLE_VALUE){
    self->error = FW_E_UNKNOWN;
    return false;
  }

  return true;
#endif
};

void fw_deinit(FW* self){
#if defined __linux
  inotify_rm_watch(self->fd, self->wd);
  close(self->fd);
#elif defined __WIN32
  CloseHandle(self->handle);
#endif
}

bool fw_watch(FW* self){
  self->received_events = 0;
  memset(self->name, 0, sizeof(self->name));

#if defined __linux

  char buffer[sizeof(struct inotify_event) + NAME_MAX];
  int n = read(self->fd, buffer, sizeof(buffer));
  if(n < 0){
    switch(errno){
      case EAGAIN: self->error = FW_E_NO_EVENT; break;
      case EACCES: self->error = FW_E_ACCESS_DENIED; break;
      case EBADF:  self->error = FW_E_UNKNOWN; break;
      case EFAULT: self->error = FW_E_BAD_STATE; break;
      case EINTR:  self->error = FW_E_NO_EVENT; break;
      case EINVAL: self->error = FW_E_BAD_STATE; break;
      case EIO:    self->error = FW_E_IO_ERROR; break;
      default:     self->error = FW_E_UNKNOWN; break;
    }
    return false;
  }
  struct inotify_event* event = (void*)buffer;
  self->received_events = event->mask & self->watch_events;
  memcpy(self->name, event->name, event->len);

  return true;
#elif defined __WIN32

  self->received_events = 0;

  while((self->received_events & self->watch_events) == 0){

    FILE_NOTIFY_INFORMATION* event = (FILE_NOTIFY_INFORMATION*)self->change_buffer;

    // get next event
    if(self->next_event == NULL){
      DWORD ret = ReadDirectoryChangesW(
          self->handle,
          self->change_buffer,
          sizeof(self->change_buffer),
          TRUE,
          FILE_NOTIFY_CHANGE_FILE_NAME
          | FILE_NOTIFY_CHANGE_DIR_NAME
          | FILE_NOTIFY_CHANGE_LAST_WRITE,
          NULL,
          &self->event_info,
          NULL);
      if(ret == 0){
        self->error = FW_E_UNKNOWN;
        return false;
      }

      ret = WaitForSingleObject(self->event_info.hEvent, INFINITE);
      if(ret != WAIT_OBJECT_0){
        switch(ret){
          case WAIT_ABANDONED:
          case WAIT_TIMEOUT: self->error = FW_E_NO_EVENT; break;
          default: self->error = FW_E_UNKNOWN; break;
        }
        return false;
      }
    }else{
      event = self->next_event;
    }

    DWORD n = 0;
    GetOverlappedResult(self->handle, &self->event_info, &n, FALSE);
    FILE_NOTIFY_INFORMATION* old_event = NULL;
    char* name_buf = self->name;
    switch(event->Action){
      case FILE_ACTION_ADDED:
        self->received_events |= FW_CREATE;
        break;
      case FILE_ACTION_REMOVED:
        self->received_events |= FW_DELETE;
        break;
      case FILE_ACTION_MODIFIED:
        self->received_events |= FW_MODIFY;
        break;
      case FILE_ACTION_RENAMED_OLD_NAME:
        self->received_events |= FW_MOVED_FROM;
        break;
      case FILE_ACTION_RENAMED_NEW_NAME:
        self->received_events |= FW_MOVED_TO;
        break;
      default: break;
    }
    
    // TODO: handle non ascii names
    DWORD name_len = event->FileNameLength/sizeof(wchar_t);
    for(DWORD i = 0; i < name_len; ++i){
      name_buf[i] = event->FileName[i];
    }

    if(event->NextEntryOffset != 0){
      self->next_event = (FILE_NOTIFY_INFORMATION*)(((char*)event) + event->NextEntryOffset);
    }else{
      self->next_event = NULL;
    }

    self->received_events &= self->watch_events;
  }

  return true;
#endif
}

const char* fw_strerror(FW_Error error){
  switch (error) {
    case FW_E_IO_ERROR: return "Platform IO error"; 
    case FW_E_PLATFORM_LIMIT: return "A platform limitiation has been reached"; 
    case FW_E_ACCESS_DENIED: return "Access to file/direcoty has been denied"; 
    case FW_E_BAD_STATE: return "FW is in bad state"; 
    case FW_E_INVALID_ARGUMENT: return "An invalid argument was provided"; 
    case FW_E_NO_EVENT: return "No event was available"; 
    case FW_E_PATH_NOT_FOUND: return "Path not found"; 
    case FW_E_PATH_TOO_LONG: return "Path is too long"; 
    case FW_E_UNKNOWN: return "An unknown error occured"; 
  }
  return "Invalid error code provided to fw_strerror";
}

FW_Error fw_error(FW* self){
  return self->error;
}

FW_Event fw_event(FW* self){
  return self->received_events;
}

const char* fw_name(FW* self){
  return self->name;
}

bool fw_once(FW* self, const char* path, FW_Event events){
  if(!fw_init(self, path, events)){
    return false;
  }
  if(!fw_watch(self)){
    return false;
  }
  fw_deinit(self);
  return true;
}
#endif // FW_IMPLEMENTATION
#endif // FW_H_
