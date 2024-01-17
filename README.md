# Resilient UDP File Transfer System

## Overview
This project implements a UDP-based client-server system for robust file transfer, designed to effectively handle unstable network conditions and unreliable file operations. The focus is on managing high 'nastiness' levels, simulating extreme network and file read/write errors, to ensure reliable data transmission.

## Features
- **UDP-Based Communication**: Utilizes User Datagram Protocol for client-server interaction.
- **Resilient to Network Unreliability**: Handles varying levels of network instability.
- **Efficient File Error Handling**: Innovatively manages file read/write errors by varying block sizes.
- **Performance-Oriented Design**: Capable of completing extensive test cases efficiently under high 'nastiness' conditions.

## Components
- `fileclient`: The client module, responsible for sending files and handling network communication.
- `fileserver`: The server module, responsible for receiving files, performing integrity checks, and managing file states.
-  `filehelper`:  utility module providing essential functions and data structures for file processing and network message handling, supporting the core functionalities of the file transfer system.

## Build Instructions
The project is built using a Makefile which compiles the `fileclient` and `fileserver` programs.

### Commands
- `make all`: Compiles all components of the project.
- `make clean`: Removes all compiled object and executable files.

## Usage
After building the project, run the `fileclient` and `fileserver` executables with the appropriate arguments.

For `fileclient`:
```bash
./fileclient <server_name> <network_nastiness> <file_nastiness> <source_directory>
```
For `fileserver`:
```bash
./fileserver <network_nastiness> <file_nastiness> <target_directory>
```
