# mouseboard

```mouseboard``` control mouse pointer by keyboard.<br>
Have full control over mouse pointer and move it smoothly using keyboard.

# Enviroments
  ```Linux```

# Features
- __Percision__ Non continuous presses to one or more of direction keys for precise mouse movement.
- __Acceleration__ Control mouse acceleration by simply keep pressing one or more of the direction keys (the longer you press the faster mouse pointer goes).
- Move mouse pointer in two directions simultaneously.
- Customize the mouse speed, acceleration and keyboard keys to control the mouse.
- Return the functionality of the keyboard keys assigned to mouse control once the program terminated.

# Build
```bash
gcc -o mouseboard mouseboard.c -lm -pthread
```

# Usage
```bash
sudo ./mouseboard
```

# Configuration
## Edit configuration
```bash
 sudo ./mouseboard -C
```
__or__

```bash
sudo ./mouseboard --config
```
> [!NOTE]
> the new configuration saved in ```mouse.cfg``` file in binary form.

## Run with default configuration

```bash
sudo ./mouseboard -D
```
__or__

```bash
sudo ./mouseboard --default
```
# How to use

## Default mouse directions keyboard keys
| Direction     | key             |
| ------------- | ----------------|
| UP            | ⬆ UP ARROW     |
| DOWN          | ⬇ DOWN ARROW   |
| LEFT          | ⬅ LEFT ARROW   |
| RIGHT         | ➡ RIGHT ARROW  |

## Default mouse buttons keyboard keys
| Buttons       | key      |
| --------------| ---------|
| RIGHT BUTTON  | KPENTER  |
| LEFT BUTTON   | KP0      |
| WHEEL UP      | KP8      |
| WHEEL DOWN    | KP2      |

*KP stand for keypad

## Default mouse speeds and acceleration
- Minimum speed (Start speed)---------[10000]
- Max speed (End speed)-------------------[700]
- Acceleration-----------------------------------[2.00]
  
> [!NOTE]
> * The lower the speed number the faster the mouse goes and vice versa.
> * The lower the acceleration number is the faster mouse accelerate.
