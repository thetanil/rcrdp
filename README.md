# rcrdp
Remote Control RDP

A persistent HTTP REST server built with FreeRDP library for remote desktop automation. Maintains a persistent RDP connection and exposes screenshot capture, keyboard input, and mouse control via simple HTTP endpoints.

## Features

- **Persistent RDP Connection**: Establishes a single RDP connection at startup and maintains it throughout the server lifetime
- **HTTP REST API**: Simple HTTP/1.1 server with JSON/binary endpoints - no authentication, compression, or extra features
- **Screenshot Endpoint**: Captures screen and returns PNG data via HTTP with automatic black pixel detection and retry logic
- **Keyboard Input**: Uses `freerdp_input_send_keyboard_event` via JSON POST requests
- **Mouse Control**: Uses `freerdp_input_send_mouse_event` for clicks and movement via JSON POST requests
- **Headless Operation**: No GUI or interactive display required
- **Performance Optimized**: No connect/disconnect overhead per operation - maintains persistent session

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

The server maintains a persistent RDP connection and exposes HTTP REST endpoints for remote control operations.

```bash
Usage: rcrdp [options]

Connection options:
  -h, --host <hostname>     RDP server hostname (required)
  -r, --rdp-port <port>     RDP server port (default: 3389)
  -u, --username <user>     Username for authentication
  -P, --password <pass>     Password for authentication
  -d, --domain <domain>     Domain for authentication

Server options:
  -p, --port <port>         HTTP server port (default: 8080)
  --help                    Show this help message
```

### HTTP API Endpoints

- **`GET /screen`** - Get current screenshot (returns PNG binary data)
- **`GET /status`** - Get connection status (returns JSON)
- **`POST /sendkey`** - Send keyboard event (accepts JSON)
- **`POST /sendmouse`** - Send mouse button event (accepts JSON)
- **`POST /movemouse`** - Move mouse cursor (accepts JSON)

## Examples

### Starting the Server

```bash
# Start HTTP server with RDP connection
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password

# Start on custom HTTP port
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password -p 8080

# Start with domain authentication
./build/bin/rcrdp -h 192.168.1.100 -u admin -P password -d MYDOMAIN
```

### API Usage Examples

#### Get Screenshot
```bash
# Save current screenshot to file
curl http://localhost:8080/screen > screenshot.png

# Get screenshot with wget
wget -O screenshot.png http://localhost:8080/screen
```

#### Get Connection Status
```bash
# Check connection status
curl http://localhost:8080/status

# Example response:
# {"connected": true,"hostname": "192.168.1.100","port": 3389,"username": "admin"}
```

#### Send Keyboard Input
```bash
# Press 'A' key (key down)
curl -X POST -d '{"flags":1,"code":65}' http://localhost:8080/sendkey

# Release 'A' key (key up)
curl -X POST -d '{"flags":2,"code":65}' http://localhost:8080/sendkey

# Press Enter key
curl -X POST -d '{"flags":1,"code":13}' http://localhost:8080/sendkey

# Press Ctrl+C (Ctrl down, C down, C up, Ctrl up)
curl -X POST -d '{"flags":1,"code":17}' http://localhost:8080/sendkey  # Ctrl down
curl -X POST -d '{"flags":1,"code":67}' http://localhost:8080/sendkey  # C down
curl -X POST -d '{"flags":2,"code":67}' http://localhost:8080/sendkey  # C up
curl -X POST -d '{"flags":2,"code":17}' http://localhost:8080/sendkey  # Ctrl up
```

#### Mouse Control
```bash
# Move mouse to coordinates (100, 200)
curl -X POST -d '{"x":100,"y":200}' http://localhost:8080/movemouse

# Left click at current position
curl -X POST -d '{"flags":4096,"x":100,"y":200}' http://localhost:8080/sendmouse

# Right click at coordinates (300, 400)
curl -X POST -d '{"flags":8192,"x":300,"y":400}' http://localhost:8080/sendmouse

# Double click
curl -X POST -d '{"flags":4096,"x":100,"y":200}' http://localhost:8080/sendmouse
curl -X POST -d '{"flags":4096,"x":100,"y":200}' http://localhost:8080/sendmouse
```

#### Common Mouse Flags
- `4096` (0x1000) - Left button click
- `8192` (0x2000) - Right button click
- `16384` (0x4000) - Middle button click

#### Common Key Codes
- `13` - Enter
- `27` - Escape  
- `32` - Space
- `65-90` - A-Z keys
- `48-57` - 0-9 keys
- `17` - Ctrl
- `16` - Shift
- `18` - Alt

## Testing

### RDP Connection Tests

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
- Screenshot functionality with black pixel detection and retry logic
- Invalid credential handling

### Manual HTTP API Testing

Once the server is running, you can test the HTTP endpoints manually:

```bash
# Start the server
./build/bin/rcrdp -h YOUR_RDP_HOST -u YOUR_USERNAME -P YOUR_PASSWORD

# In another terminal, test the endpoints:

# Check if server is responding
curl http://localhost:8080/status

# Get a screenshot
curl http://localhost:8080/screen > test.png

# Send some keyboard input
curl -X POST -d '{"flags":1,"code":65}' http://localhost:8080/sendkey

# Move mouse and click
curl -X POST -d '{"x":200,"y":300}' http://localhost:8080/movemouse
curl -X POST -d '{"flags":4096,"x":200,"y":300}' http://localhost:8080/sendmouse
```

## Dependencies

- FreeRDP 3.x development libraries
- WinPR 3.x libraries  
- libpng development libraries
- GCC compiler
- Make
