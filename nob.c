
#define NOB_IMPLEMENTATION
#include "nob.h"

int main(int argc, char** argv){
  NOB_GO_REBUILD_URSELF(argc, argv);

  const char* program = nob_shift_args(&argc, &argv);
  Nob_Cmd cmd = {0};

  const char* command = NULL;
  if(argc > 0){
    command = nob_shift_args(&argc, &argv);
  }

  const char* target = "./app";
  if(command != NULL 
      && strcmp(command, "cross") == 0
  ){
    target = "./app.exe";
    nob_cmd_append(&cmd, "x86_64-w64-mingw32-gcc");
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, target);
    //nob_cc_inputs(&cmd, "main.c");
    nob_cc_inputs(&cmd, "main.c");
    nob_cmd_append(&cmd, "-lshlwapi");
  }else{
    nob_cc(&cmd);
    nob_cc_flags(&cmd);
    nob_cc_output(&cmd, target);
    nob_cc_inputs(&cmd, "main.c");
  }
  
  if(!nob_cmd_run(&cmd)) return 1;

  nob_cmd_append(&cmd, target);

  if(!nob_cmd_run(&cmd)) return 1;

  return 0;
}
