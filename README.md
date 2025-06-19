# rcrdp
Remote Control RDP

A CLI tool built with FreeRDP library for remote desktop automation. Supports screenshot capture, keyboard input, and mouse control without requiring an interactive display or GUI window.

## Features

- **Core RDP Connection**: Uses FreeRDP 3.x library for establishing RDP connections
- **CLI Interface**: Full command-line interface with argument parsing
- **Screenshot Command**: Captures screen and saves as PNG format with auto-generated ISO timestamps
- **SendKey Command**: Uses `freerdp_input_send_keyboard_event` for keyboard input
- **SendMouse Command**: Uses `freerdp_input_send_mouse_event` for mouse clicks  
- **MoveMouse Command**: Uses `freerdp_input_send_mouse_event` for mouse movement
- **Headless Operation**: No GUI or interactive display required

## Building

### Using Make (recommended)
```bash
make
```
The executable will be built to `build/bin/rcrdp`.

### Using CMake
```bash
mkdir build && cd build
cmake ..
make
```
The executable will be built to `build/bin/rcrdp`.

## Usage

```bash
Usage: rcrdp [options] <command> [command_args]

Connection options:
  -h, --host <hostname>     RDP server hostname
  -p, --port <port>         RDP server port (default: 3389)
  -u, --username <user>     Username for authentication
  -P, --password <pass>     Password for authentication
  -d, --domain <domain>     Domain for authentication

Commands:
  connect                   Connect to RDP server
  disconnect                Disconnect from RDP server
  screenshot [file.png]     Take screenshot and save as PNG file (auto-generated filename if not provided)
  sendkey <flags> <code>    Send keyboard event
                            flags: 1=down, 2=release
                            code: virtual key code
  sendmouse <flags> <x> <y> Send mouse event
                            flags: mouse button/action flags
  movemouse <x> <y>         Move mouse to coordinates
```

## Examples

```bash
# Connect to RDP server
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password connect

# Take screenshot with auto-generated filename (saved to png/ directory)
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password screenshot

# Take screenshot with custom filename 
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password screenshot desktop.png

# Send keyboard events (press/release 'A' key)
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password sendkey 1 65
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password sendkey 2 65

# Move mouse and click
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password movemouse 100 200  
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password sendmouse 0x1000 100 200
```

## Testing

To run integration tests that verify RDP connection functionality:

1. Copy the example environment file:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` and configure your test RDP server details:
   ```bash
   RCRDP_TEST_HOST=192.168.1.100
   RCRDP_TEST_USER=testuser
   RCRDP_TEST_PASS=testpassword
   RCRDP_TEST_PORT=3389
   RCRDP_TEST_DOMAIN=
   ```

3. Run the tests:
   ```bash
   make test
   ```

The tests will verify:
- Basic RDP connection establishment
- Connection state management
- Proper disconnection
- Connection lifecycle (multiple connect/disconnect cycles)
- Invalid credential handling

## Dependencies

- FreeRDP 3.x development libraries
- WinPR 3.x libraries  
- libpng development libraries
- GCC compiler
- Make
