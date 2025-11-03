/*
 * MIT License
 * 
 * Copyright (c) 2025 Alaric de Ruiter
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FW_H_
#define FW_H_
#include <string.h>
#include <stdbool.h>

#if defined(__linux)
#include <sys/inotify.h>
#include <linux/limits.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#define FW_NAME_MAX NAME_MAX
#elif defined(__WIN32)
#include <windows.h>
#include <fileapi.h>
#include <shlwapi.h>
#define FW_NAME_MAX MAX_PATH 
#else
#error "Platform not supported"
#endif

typedef enum{
  FW_CREATE = (1<<0),
  FW_DELETE = (1<<1),
  FW_MODIFY = (1<<2),
  FW_RENAME = (1<<3),
  FW_ALL = FW_CREATE
    | FW_DELETE
    | FW_MODIFY
    | FW_RENAME,
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
  FW_E_INCOMPLETE_EVENT,
  FW_E_IO_ERROR,
} FW_Error;

typedef struct{
  FW_Error error;
  FW_Event watch_events;
  FW_Event received_events;
  char name[FW_NAME_MAX+1];
  char new_name[FW_NAME_MAX+1];
  
  char event_buffer[1024];

#if defined(__linux)
  int fd;
  int wd;
  int bytes_left;
  struct inotify_event* event;

#elif defined(__WIN32)
  HANDLE handle;
  FILE_NOTIFY_INFORMATION* event;
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
const char* fw_new_name(FW* self);

// --- error handling ---
const char* fw_strerror(FW_Error error);
FW_Error fw_error(FW* self);

#ifdef FW_IMPLEMENTATION
bool fw_init(FW* self, const char* path, FW_Event events){
  memset(self, 0, sizeof(*self));
  self->watch_events = events;

#if defined(__linux)

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

  int in_events = 0;
  if(events & FW_CREATE) in_events |= IN_CREATE;
  if(events & FW_DELETE) in_events |= IN_DELETE;
  if(events & FW_MODIFY) in_events |= IN_MODIFY;
  if(events & FW_RENAME) in_events |= IN_MOVE;

  self->wd = inotify_add_watch(self->fd, path, in_events);
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

#elif defined(__WIN32)

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
#if defined(__linux)
  inotify_rm_watch(self->fd, self->wd);
  close(self->fd);
#elif defined(__WIN32)
  CloseHandle(self->handle);
#endif
}

bool fw__event_queue_is_empty(FW* self){
#if defined(__linux)
  return self->bytes_left == 0;
#elif defined(__WIN32)
  return self->event == NULL;
#endif
}

void fw__consume_event(FW* self, char* name_buf){
#if defined(__linux)
  self->event = (struct inotify_event*) self->event_buffer;
  if(name_buf != NULL){
    memcpy(name_buf, self->event->name, self->event->len);
  }
  int event_size = sizeof(*self->event)+self->event->len;
  self->bytes_left -= event_size;
  assert(self->bytes_left >= 0);
  for(int i = 0; i < self->bytes_left; ++i){
    self->event_buffer[i] = self->event_buffer[i+event_size];
  }

#elif defined(__WIN32)
  if(name_buf != NULL){
    // TODO: handle non ascii names
    DWORD name_len = self->event->FileNameLength/sizeof(wchar_t);
    for(DWORD i = 0; i < name_len; ++i){
      name_buf[i] = self->event->FileName[i];
    }
  }

  if(self->event->NextEntryOffset != 0){
    self->event = (FILE_NOTIFY_INFORMATION*)(((char*)self->event) + self->event->NextEntryOffset);
  }else{
    self->event = NULL;
  }
#endif
}

bool fw_watch(FW* self){
  if(self->watch_events == 0){
    self->error = FW_E_NO_EVENT;
    return false;
  }

  self->received_events = 0;
  memset(self->name, 0, sizeof(self->name));
  memset(self->new_name, 0, sizeof(self->new_name));

#if defined(__linux)

  while(self->received_events == 0){
    if(self->bytes_left == 0){
      int n = read(self->fd, self->event_buffer, sizeof(self->event_buffer));
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
      self->bytes_left = n;
    }

    while(self->bytes_left > 0){
      struct inotify_event* event = (void*)self->event_buffer;
      switch(event->mask){
        case IN_CREATE:
          self->received_events = FW_CREATE;
          fw__consume_event(self, self->name);
          return true;
        case IN_DELETE:
          self->received_events = FW_DELETE;
          fw__consume_event(self, self->name);
          return true;
        case IN_MODIFY:
          self->received_events = FW_MODIFY;
          fw__consume_event(self, self->name);
          return true;
        case IN_MOVED_FROM:
          fw__consume_event(self, self->name);
          if(strlen(self->new_name) > 0){
            self->received_events = FW_RENAME;
            return true;
          }else if(fw__event_queue_is_empty(self)){
            self->received_events = FW_RENAME;
            // new name was not received but
            // no more events are available
            self->error = FW_E_INCOMPLETE_EVENT;
            return true;
          }
          break;
        case IN_MOVED_TO:
          fw__consume_event(self, self->new_name);
          if(strlen(self->name) > 0){
            self->received_events = FW_RENAME;
            return true;
          }else if(fw__event_queue_is_empty(self)){
            self->received_events = FW_RENAME;
            // old name was not received but
            // no more events are available
            self->error = FW_E_INCOMPLETE_EVENT;
            return true;
          }
          break;
        default: break;
      }
    }
  }
  return false;
#elif defined(__WIN32)

  self->received_events = 0;

  while(true){

    // get next event
    if(self->event == NULL){
      DWORD ret = ReadDirectoryChangesW(
          self->handle,
          self->event_buffer,
          sizeof(self->event_buffer),
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
      self->event = (FILE_NOTIFY_INFORMATION*)self->event_buffer;
    }

    DWORD n = 0;
    GetOverlappedResult(self->handle, &self->event_info, &n, FALSE);
    switch(self->event->Action){
      case FILE_ACTION_ADDED:
        if(self->watch_events & FW_CREATE){
          self->received_events = FW_CREATE;
          fw__consume_event(self, self->name);
          return true;
        }else{
          fw__consume_event(self, NULL);
        }
        break;
      case FILE_ACTION_REMOVED:
        if(self->watch_events & FW_DELETE){
          self->received_events = FW_DELETE;
          fw__consume_event(self, self->name);
          return true;
        }else{
          fw__consume_event(self, NULL);
        }
        break;
      case FILE_ACTION_MODIFIED:
        if(self->watch_events & FW_MODIFY){
          self->received_events = FW_MODIFY;
          fw__consume_event(self, self->name);
          return true;
        }else{
          fw__consume_event(self, NULL);
        }
        break;
      case FILE_ACTION_RENAMED_OLD_NAME:
        if(self->watch_events & FW_RENAME){
          self->received_events = FW_RENAME;
          fw__consume_event(self, self->name);
          if(strlen(self->new_name) > 0){
            self->received_events = FW_RENAME;
            return true;
          }else if(fw__event_queue_is_empty(self)){
            // new name was not received but
            // no more events are available
            self->error = FW_E_INCOMPLETE_EVENT;
            return true;
          }
        }else{
          fw__consume_event(self, NULL);
        }
        break;
      case FILE_ACTION_RENAMED_NEW_NAME:
        if(self->watch_events & FW_RENAME){
          self->received_events = FW_RENAME;
          fw__consume_event(self, self->new_name);
          if(strlen(self->name) > 0){
            return true;
          }else if(fw__event_queue_is_empty(self)){
            // old name was not received but
            // no more events are available
            self->error = FW_E_INCOMPLETE_EVENT;
            return true;
          }
        }else{
          fw__consume_event(self, NULL);
        }
        break;
      default: break;
    }
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
    case FW_E_INCOMPLETE_EVENT: return "An incomplete (FW_RENAME) event was received"; 
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

const char* fw_new_name(FW* self){
  return self->new_name;
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
