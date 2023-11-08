# Project converter

Simple modular python script to convert existing projects from IAR's ewp and ARM's KEIL uvprojx project formats to cmake and linker file using GCC compiler.

## Module description

cmake.py - Cmake and linker file generation
converter.py - Argument parsing
ewpproject.py - Parser for IAR's ewp file format
uvprojx.py - Parser for ARM's KEIL uvprojx file format

## Usage

Run in output dir.

Convert project from IAR:

    converter.py ewp <path to project root>

Convert project from ARM's KEIL:

    converter.py uvprojx <path to project root>
	
## TODO

[] Seperate templates into submodule
[] Support generation of Makefile
[] Support additional compilers
[] Test on MAC OSX 
[] Arg to specify build directory
[] Add Tests