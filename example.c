#include <stdio.h>

#define FW_IMPLEMENTATION
#include "fw.h"

int main(int argc, char** argv){

  const char* program = *argv;
  argv++;
  argc--;

  const char* watch_path = ".";

  if(argc > 0){
    watch_path = *argv;
  }

  FW fw;
  if(fw_init(&fw, watch_path, FW_CREATE
        | FW_MODIFY 
        | FW_DELETE
        | FW_RENAME
      ) == false
  ){
    printf("init failed %s\n", fw_strerror(fw_error(&fw)));
    return 1;
  }

  while(true){
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
      return 1;
    }
  }

  return 0;
}
