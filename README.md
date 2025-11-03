# fw.h
Simple cross-platform file watcher

## Currently Supported Platforms

- Windows (win32)
- Linux (inotify)

## Simple Example

```C
#define FW_IMPLEMENTATION
#include "fw.h"

int main(void){
  FW fw;
  const char* watch_path = ".";

  if(fw_init(&fw, watch_path, FW_CREATE
        | FW_MODIFY 
        | FW_DELETE
        | FW_RENAME
      ) == false
  ){
    printf("init failed %s\n", fw_strerror(fw_error(&fw)));
    return 1;
  }

  if(fw_watch(&fw)){
    if(fw_event(&fw) & FW_CREATE){
      printf("created: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_MODIFY){
      printf("modified: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_DELETE){
      printf("deleted: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_RENAME){
      printf("rename: %s -> %s\n", fw_name(&fw), fw_new_name(&fw));
    }
  }else{
    printf("watch failed %s\n", fw_strerror(fw_error(&fw)));
    fw_deinit(&fw);
    return 1;
  }

  fw_deinit(&fw);

  return 0;
}
```

### Compact but less robust example

The above example is quite complete and robust but if you don't require that robustness and just quickly want to throw something together that waits on a change in a directory a single `fw_once` call can be used.

This is less robust in the sense that it is more likely to miss events because the context is only active during the `fw_once` call during which only 1 event can be received and outside which events are not recorded.

```C
#define FW_IMPLEMENTATION
#include "fw.h"

int main(void){
  FW fw;
  const char* watch_path = ".";

  if(fw_once(&fw, watch_path, FW_ALL){
    if(fw_event(&fw) & FW_CREATE){
      printf("created: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_MODIFY){
      printf("modified: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_DELETE){
      printf("deleted: %s\n", fw_name(&fw));
    }else if(fw_event(&fw) & FW_RENAME){
      printf("rename: %s -> %s\n", fw_name(&fw), fw_new_name(&fw));
    }
  }else{
    printf("watch once failed %s\n", fw_strerror(fw_error(&fw)));
    return 1;
  }

  return 0;
}
```

## Polling event functions

The following function are used to poll for events.

| Function | Description |
|-|-|
| `bool fw_init(FW*, const char* path, FW_Event events)` | Initializes the context to watch `path` for the given `FW_Event`s. Returns `false` on error. |
| `bool fw_watch(FW*)` | Watches the given `path` provided in `fw_init` and only returns when one of the specified events occured or an error occured. Returns `false` on error. |
| `void fw_deinit(FW*)` | Deinitializes the given context and cleans up any resources allocated by the context. Because event and error data is stored in the `FW` structure this is left accessible using the `fw_event`, `fw_name`, `fw_new_name` and `fw_error` functions. |
| `bool fw_once(FW*, const char* path, FW_Event events)` | Performs `fw_init` with the given arguments and if succesfull calls `fw_watch` and `fw_deinit` in that order. Leaving the user with deinitialized context still containing valid event and or error data (depending on the return value). Returns `false` on error. |

## Events

| Event | Description |
|-|-|
| `FW_CREATE` | Received when a file is created. |
| `FW_DELETE` | Received when a file is deleted. |
| `FW_MODIFY` | Received when a file is modified. |
| `FW_RENAME` | Received when a file is renamed. |
| `FW_ALL`  | Enables all events when passed to `fw_init`, cannot be itself received as event. |

An important note about `FW_RENAME` is that sometimes the OS may not report the old or new name of the file if it is from or to a location outside of the monitored directoy.

In this case one of two thing can happend:
- FW returns FW_CREATE and or FW_DELETE instead of FW_RENAME.
- (TODO check if this actually happens) `fw_watch` returns true with FW_RENAME set but also `FW_E_INCOMPLETE_EVENT` set and either `fw_name` or `fw_new_name` return a zero-length string.

## Get Event Information

The following function can be used to get event information from the `FW` context.

| Getter | Description |
|-|-|
| `FW_Event fw_event(FW*)` | The event that was received. |
| `const char* fw_name(FW*)` | Name of the affected file. |
| `const char* fw_new_name(FW*)` | New name of file if it has been renamed, in this case the old name is accessible using `fw_name`. |

## Error Handling

`FW_Error` codes can be retrieved using `fw_error(FW*)`.
Though, current error codes are not yet stable but can still be used for debugging through their textual representation using `const char* fw_strerror(FW_Error)`.
