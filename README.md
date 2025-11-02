# fw.h
Simple cross-platform file watcher

## Simplest Example

The following example shows how to wait on a single specified `FW_Event` using a single function.
This easiest if you need to just wait on a change in a directory for example, but this method may
miss events if more than 1 is received at the same time or between `fw_once` calls.

```C
#define FW_IMPLEMENTATION
#include "fw.h"

int main(void){

    FW fw;
    const char* watch_path = ".";

    while(true){
        if(fw_once(&fw, watch_path, FW_CREATE | FW_DELETE)){
            if(fw_event(&fw) & FW_CREATE){
                printf("file created: %s", fw_name(&fw));
            }else if(fw_event(&fw) & FW_DELETE){
                printf("file deleted: %s", fw_name(&fw));
            }
        }else{
            printf("FW error: %s", fw_strerror(fw_error(&fw)));
            return 1;
        }
    }

    return 0;
}
```

For a slightly more robust example read further down below.

## Polling event functions

| Function | Description |
|-|-|
| `bool fw_init(FW*, const char* path, FW_Event events)` | Initializes the context to watch `path` for the given `FW_Event`s |
| `bool fw_watch(FW*)` | Watches the given `path` provided in `fw_init` and only returns when one of the specified events occured or an error occured. |
| `void fw_deinit(FW*)` | Deinitializes the given context and cleans up any resources allocated by the context. Because event and error data is stored in the `FW` structure this is left accessible using the `fw_event`, `fw_name` and `fw_error` functions. |
| `bool fw_once(FW*, const char* path, FW_Event events)` | Performs `fw_init` with the given arguments and if succesfull calls `fw_watch` and `fw_deinit` in that order. Leaving the user with deinitialized context still containing valid event and or error data (depending on the return value). |

## Events

FW provides support for 5 different kinds of events:

| Event | Description |
|-|-|
| `FW_CREATE` | Received when a file is created |
| `FW_DELETE` | Received when a file is deleted |
| `FW_MODIFY` | Received when a file is modified |
| `FW_MOVED_FROM` | Received when a file is about to receive a new name |
| `FW_MOVED_TO` | Usually received right after `FW_MOVED_FROM` when a file has changed its name |

## Get Event Information

The following function can be used to get event information from the `FW` context.

| Getter | Description |
|-|-|
| `FW_Event fw_event(FW*)` | 
| `const char* fw_name(FW*)` | Name of the affected file |

## Error Handling

`FW_Error` codes can be retrieved using `fw_error(FW*)`.
Though, current error codes are not yet stable but can still be used for debugging through their textual representation using `const char* fw_strerror(FW_Error)`.

## Slightly more useful and robust example

`fw_once` initializes but also uninitializes the `FW` context object.
This means that in some cases events already received by this context are lost.
This is most likely in the case of `FW_MOVED_TO` and `FW_MOVED_FROM` events since these are usually generated
by a single file move event and thus generated at the same time.


In order to catch all events the context should be preserved until you no longer wish to watch the given path.
This can be done using a combination of `fw_init`, `fw_watch` and `fw_deinit`.

```C
#define FW_IMPLEMENTATION
#include "fw.h"

int main(void){

    FW fw;
    const char* watch_path = ".";
    if(fw_init(&fw, watch_path, FW_CREATE | FW_DELETE) == false){
        printf("FW init error: %s", fw_strerror(fw_error(&fw)));
        return 1;
    }

    while(true){
        if(fw_watch(&fw)){
            if(fw_event(&fw) & FW_CREATE){
                printf("file created: %s", fw_name(&fw));
            }else if(fw_event(&fw) & FW_DELETE){
                printf("file deleted: %s", fw_name(&fw));
            }
        }else{
            printf("FW error: %s", fw_strerror(fw_error(&fw)));
            fw_deinit(&fw);
            return 1;
        }
    }

    return 0;
}
```
