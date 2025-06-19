# Mouse Button Flags Reference

This document shows exactly where the mouse button flags are defined and how to use them.

## Source of Definitions

### Primary Header File
**File:** `/usr/include/freerdp3/freerdp/input.h`

### FreeRDP Repository
**Online:** https://github.com/FreeRDP/FreeRDP/blob/master/include/freerdp/input.h

## Standard Pointer Flags

From `/usr/include/freerdp3/freerdp/input.h` lines 39-47:

```c
/* Pointer Flags */
#define PTR_FLAGS_HWHEEL 0x0400
#define PTR_FLAGS_WHEEL 0x0200
#define PTR_FLAGS_WHEEL_NEGATIVE 0x0100
#define PTR_FLAGS_MOVE 0x0800
#define PTR_FLAGS_DOWN 0x8000
#define PTR_FLAGS_BUTTON1 0x1000 /* left */
#define PTR_FLAGS_BUTTON2 0x2000 /* right */
#define PTR_FLAGS_BUTTON3 0x4000 /* middle */
```

## Extended Pointer Flags

From `/usr/include/freerdp3/freerdp/input.h` lines 50-52:

```c
/* Extended Pointer Flags */
#define PTR_XFLAGS_DOWN 0x8000
#define PTR_XFLAGS_BUTTON1 0x0001
#define PTR_XFLAGS_BUTTON2 0x0002
```

## Practical Usage in rcrdp

### Standard Mouse Clicks (Recommended)

**Left Click:**
- Flag: `PTR_FLAGS_BUTTON1 | PTR_FLAGS_DOWN`
- Value: `0x1000 | 0x8000 = 0x9000 = 36864`
- Usage: `curl -X POST -d '{"flags":36864,"x":100,"y":200}' http://localhost:8080/sendmouse`

**Right Click:**
- Flag: `PTR_FLAGS_BUTTON2 | PTR_FLAGS_DOWN`
- Value: `0x2000 | 0x8000 = 0xA000 = 40960`
- Usage: `curl -X POST -d '{"flags":40960,"x":300,"y":400}' http://localhost:8080/sendmouse`

**Middle Click:**
- Flag: `PTR_FLAGS_BUTTON3 | PTR_FLAGS_DOWN`
- Value: `0x4000 | 0x8000 = 0xC000 = 49152`
- Usage: `curl -X POST -d '{"flags":49152,"x":500,"y":300}' http://localhost:8080/sendmouse`

### Button Release (for complete sequences)

**Left Release:**
- Flag: `PTR_FLAGS_BUTTON1` (no DOWN flag)
- Value: `0x1000 = 4096`

**Right Release:**
- Flag: `PTR_FLAGS_BUTTON2` (no DOWN flag)
- Value: `0x2000 = 8192`

**Middle Release:**
- Flag: `PTR_FLAGS_BUTTON3` (no DOWN flag)
- Value: `0x4000 = 16384`

### Mouse Movement

**Mouse Move:**
- Flag: `PTR_FLAGS_MOVE`
- Value: `0x0800 = 2048`
- Usage: `curl -X POST -d '{"x":100,"y":200}' http://localhost:8080/movemouse`

## Flag Calculation

The flags are bitwise combinations:

1. **Choose button:** `PTR_FLAGS_BUTTON1` (0x1000), `PTR_FLAGS_BUTTON2` (0x2000), or `PTR_FLAGS_BUTTON3` (0x4000)
2. **Add down state:** `PTR_FLAGS_DOWN` (0x8000) for button press
3. **Combine with OR:** `button_flag | PTR_FLAGS_DOWN`

Example for right-click:
```
PTR_FLAGS_BUTTON2 | PTR_FLAGS_DOWN
= 0x2000 | 0x8000
= 0xA000
= 40960 (decimal)
```

## Debug Output

Our implementation shows detailed flag analysis:

```
DEBUG: Analyzing mouse flags 0x0000A000:
  - PTR_FLAGS_DOWN (0x8000)
  - PTR_FLAGS_BUTTON2 (0x2000) - Right
DEBUG: Sending mouse event - right_button at coordinates (300,400)
```

## Official Documentation

- **FreeRDP GitHub:** https://github.com/FreeRDP/FreeRDP
- **API Documentation:** https://github.com/FreeRDP/FreeRDP/tree/master/include/freerdp
- **Input Header:** https://github.com/FreeRDP/FreeRDP/blob/master/include/freerdp/input.h
- **RDP Protocol Spec:** Microsoft's RDP protocol documentation

## Testing

Use our debug scripts to verify flag behavior:
- `./simple_rightclick.sh` - Basic right-click test with flag analysis
- `./debug_rightclick_flags.sh` - Comprehensive flag testing
- `./test_menu_timing.sh` - Menu timing analysis

## Notes

1. **Standard vs Extended Flags:** Use standard `PTR_FLAGS_*` for compatibility
2. **Button States:** Most operations need `PTR_FLAGS_DOWN` combined with button flag
3. **Complete Sequences:** Some applications may require DOWN followed by UP events
4. **Menu Behavior:** Context menus may auto-select first item quickly in RDP sessions