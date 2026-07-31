# CASM-langage
A custom programming language inspired by assembly, with a compiler and virtual machine written in C++.

## FEATURES
- Custom assembly syntax
- Compiler that translates `.casm` to binary `.bin`
- Virtual machine to execute compiled binaries
- Dump tool for debugging

## USAGE
- CASM tools work in the console
- To use the compiler, write:
  casm <file_name>.casm <file_name>.bin
- To use the virtual machine, write  (warning! the virtual machine uses only little indian's binaries):
  casm_vm <file_name>.bin
- To use the dump, write:
  casm_dump <file_name>.bin
- To use the emulator, write:
  casm_emulator <window_scale> <file_name.bin>
- WARNING! To use commands, you should have configured your computer's path.

## STATUT
- All parts of the projet (the compiler, the dump, the virtual machine and the emulator) work.
- The VM isn't actualised
- The disks folder, the file doorOS.Casm and the file doorOS_kernel are a project in developpement
- WARNING!!! You must compile sources to use casm
- Thanks to Raylib for this project: https://github.com/raysan5/raylib
