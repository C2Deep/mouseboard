#include<stdio.h>
#include<linux/uinput.h>
#include<linux/input.h>
#include<string.h>
#include<sys/stat.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<ctype.h>
#include<math.h>
#include<termios.h>
#include<pthread.h>

#define UP                  0x01       // Up mouse movement flag
#define DOWN                0x02       // Down mouse movement flag
#define RIGHT               0x04       // Right mouse movement flag
#define LEFT                0x08       // Left mouse movement flag
#define RIGHT_BUTTON        0x10       // Mouse right button
#define LEFT_BUTTON         0x20       // Mouse left button
#define GEAR_UP_BUTTON      0x40       // Mouse wheel up
#define GEAR_DOWN_BUTTON    0X80       // Mouse wheel down

#define MAX_DEVICE_NAME    256
#define MAX_FILE_PATH      256
#define MAX_KEY            32           // Maximum number of characters to hold the key id
#define EVENTS_BUFFER      1024         // The size of thread events array

int create_mouse_device(void);                        // Virtual Mouse
int create_keyboard_device(void);                     // Virtual keyboard
void emit(int fd, int type, int code, int val);       // Emit mouse events
char *find_device(char *deviceName);                  // Find device event file
char mouse_status(struct input_event *pEvent);        // Mouse buttons and movement status
void mouse_control(int fd, char mouseStatus, struct input_event *pEvent);   // Emit the new
int mouse_speed(struct input_event *pEvent);          // Update the speed of the mouse value
void wait_keys_release(int KBfd);                     // Wait till none of keyboard keys is pressed
void mouse_config(char *cmdArg, char *kbPath);        // Custom mouse configuration (buttons wheel and movement keys)
void default_config(void);                            // Default mouse configuration
void terminal_echo(int echoStatus);                   // Control terminal echo
int flush_KB(int KBfd, char *kbPath);                 // Flush the non written unwanted keys
void key_id(int code, char* key);                     // Convert key keycode to text
void clear_screen(void);                              // Clear the screen
void *write_KBevent(void *kbPath);                    // Thread function filter the pressed keyboard keys that assigned to control mouse...
                                                      // ...and pass them to main()
void help(char *cmdArg);                               // Simple help message to use the program

struct MouseSpeedConfigure
{
    int maxSpeed;
    int minSpeed;
    float acc;          // Acceleration
};

struct MouseButtonsConfigure
{
    // Hold key codes
    int up;
    int down;
    int right;
    int left;
    int rBtn;
    int lBtn;
    int scrollUp;
    int scrollDown;

    // Hold key scan codes
    int ups;
    int downs;
    int rights;
    int lefts;
    int rBtns;
    int lBtns;
    int scrollUps;
    int scrollDowns;

};

struct Configure
{
    struct MouseSpeedConfigure msc;
    struct MouseButtonsConfigure mbc;
}cfg;

// The main() and write_KBevent() communicate using EventShare structure
struct EventShare
{
    struct input_event events[EVENTS_BUFFER];    // Hold mouse control keys
    int cWrite;                                  // Count how many elements written to the events array
    int cRead;                                   // Count how many elements read from the events array
    int diff;                                    // diff = cWrite - cRead
    int exitFlag;                                // exitFlag = 1 when ESC key pressed
}es = { .cWrite = 0, .cRead = 0 , .diff = 0, .exitFlag = 0};


int main(int argc, char *argv[])
{
    int  Mfd;
    const char CHUNCK = sizeof(struct input_event);
    char mStat = 0;
    char *kbPath = NULL;

    struct input_event ie;

    pthread_t kbWriteThread;

    kbPath = find_device("Keyboard");           // Find keyboard event file path from /dev/input

    if(argv[1] && (!(strcmp(argv[1], "-h")) || !(strcmp(argv[1], "--help"))) )
        help(argv[0]);

    // Configure the Mouse
    mouse_config(argv[1], kbPath);

    printf("Press ESC key to exit.\n");

    if((Mfd = create_mouse_device()) < 0)
    {
        fprintf(stderr, "Couldn't create mouse virtual device.\n");
        return -1;
    }


    pthread_create(&kbWriteThread, NULL, write_KBevent, (void *)kbPath);

    for ( ; !es.exitFlag || es.diff; )
    {

        if(es.diff)
        {
            ie = es.events[es.cRead];
            es.cRead = ++es.cRead % EVENTS_BUFFER;
            --es.diff;
        }

        mStat = mouse_status(&ie);
        mouse_control(Mfd, mStat, &ie);
        usleep(mouse_speed(&ie));
    }

   /*
    * Give userspace some time to read the events before we destroy the
    * device with UI_DEV_DESTROY.
    */
   sleep(1);
   ioctl(Mfd, UI_DEV_DESTROY);
   close(Mfd);

   pthread_join(kbWriteThread, NULL);
   free(kbPath);

   return 0;
}

// Name                 : emit
// Parameters           : fd        > File descriptor to emit the event to
//                        type      > Event type
//                        code      > Event code
//                        val       > Event value
// Call                 : write()
// Called by            : mouse_control()
// Return               : void
// Description          : Emit mouse events to control mouse virtual device

void emit(int fd, int type, int code, int val)
{
    struct input_event ie;

    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    write(fd, &ie, sizeof(ie));
}

// Name                 : create_mouse_device
// Parameters           : void
// Call                 : open(), ioctl(), memset(), strcpy(), sleep()
// Called by            : main()
// Return               : File descriptor of the virtual mouse
// Description          : Create virtual mouse device to emulate mouse

int create_mouse_device(void)
{
    struct uinput_setup usetup;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);

    /* enable mouse buttons */
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);

    // Enable mouse relative events
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_X);
    ioctl(fd, UI_SET_RELBIT, REL_Y);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "Mouse_Device_Emulator");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);

    /*
    * On UI_DEV_CREATE the kernel will create the device node for this
    * device. We are inserting a pause here so that userspace has time
    * to detect, initialize the new device, and can start listening to
    * the event, otherwise it will not notice the event we are about
    * to send.
    */

    sleep(1);

    return fd;
}

// Name                 : create_keyboard_device
// Parameters           : void
// Call                 : open(), ioctl(), memset(), strcpy(), sleep()
// Called by            : write_KBevent()
// Return               : File descriptor of the virtual keyboard
// Description          : Create virtual keyboard device to pass all keyboard keys except the keys assigned for mouse control

int create_keyboard_device(void)
{
    struct uinput_setup usetup;

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    ioctl(fd, UI_SET_EVBIT, EV_MSC);
    ioctl(fd, UI_SET_MSCBIT, MSC_SCAN);

     /* enable keyboard keys */
    ioctl(fd, UI_SET_EVBIT, EV_KEY);

    for(int i = 0 ; i < KEY_MAX ; ++i)
        ioctl(fd, UI_SET_KEYBIT, i);

    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x4321; /* sample vendor */
    usetup.id.product = 0x8765; /* sample product */
    strcpy(usetup.name, "Keyboard_Device_Emulator");

    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);

    // pause here so that userspace has time to detect, initialize the new device, and can start listening to the event
    sleep(1);

    return fd;
}

// Name                 : find_device
// Parameters           : char *deviceName    > name of the device that
// Call                 : malloc(), sprintf(), open(), ioctl(), free(), perror(), exit()
//                        toupper(), strstr()
// Called by            : main()
// Return               : The full event file path of the "deviceName"
// Description          : Find which event file from /dev/input event files are correspond to "deviceName"

char *find_device(char *deviceName)
{
    int fd = -1;
    char devName[MAX_DEVICE_NAME];
    char deviceNameTmp[MAX_DEVICE_NAME];

    char *eventFile = malloc(MAX_FILE_PATH);

    for(int i = 0; ; ++i)
    {
        sprintf(eventFile, "%s%d", "/dev/input/event", i);

        if((fd = open(eventFile, O_RDONLY)) < 0)
            break;

        if(ioctl(fd, EVIOCGNAME(sizeof(devName)), devName) < 0)
        {
            free(eventFile);
            perror("evdev ioctl -> EVIOCGNAME");
            exit(1);
        }

        for(int i = 0 ; devName[i] ; ++i)
            { devName[i] = toupper(devName[i]); }

        for(int i = 0 ; ; ++i)
        {
            if(!deviceName[i])
            {
                deviceNameTmp[i] = '\0';
                break;
            }
            deviceNameTmp[i] = toupper(deviceName[i]);
        }

        if(strstr(devName, deviceNameTmp))
            return eventFile;   // Device Found
    }

    // Device NOT found
    free(eventFile);
    return NULL;

}

// Name                 : mouse_status
// Parameters           : pEvent            > hold Keyboard key event that assigned to the virtual mouse
// Call                 : void
// Called by            : main()
// Return               : 1 byte (char) that hold the status of the virtual mouse
// Description          : Update the mouse movement and buttons state

char mouse_status(struct input_event *pEvent)
{
    static char mouseStatus = 0;

    if(pEvent->type == 1)
    {
        // If key relased or new key pressed
        if(pEvent->value == 0 || pEvent->value == 1)
        {
            if(pEvent->code == cfg.mbc.up)
                mouseStatus ^= UP;
            else if(pEvent->code == cfg.mbc.down)
                mouseStatus ^= DOWN;
             else if(pEvent->code == cfg.mbc.right)
                mouseStatus ^= RIGHT;
             else if(pEvent->code == cfg.mbc.left)
                mouseStatus ^= LEFT;
             else if(pEvent->code == cfg.mbc.rBtn)
                mouseStatus ^= RIGHT_BUTTON;
             else if(pEvent->code == cfg.mbc.lBtn)
                mouseStatus ^= LEFT_BUTTON;
             else if(pEvent->code == cfg.mbc.scrollUp)
                mouseStatus ^= GEAR_UP_BUTTON;
             else if(pEvent->code == cfg.mbc.scrollDown)
                mouseStatus ^= GEAR_DOWN_BUTTON;
        }
    }
    return mouseStatus;
}

// Name                 : mouse_control
// Parameters           : fd            > Virtual mouse device file descriptor
//                        mStat         > The mouse status
//                        pEevent       > hold Keyboard key event that assigned to the virtual mouse
// Call                 : emit()
// Called by            : main()
// Return               : void
// Description          : Control the mouse movement, buttons and wheel

void mouse_control(int fd, char mStat, struct input_event *pEvent)
{
    static char btnStat = 0;

    if( (mStat & UP) || (mStat & RIGHT) )
    {
        emit(fd, EV_REL, REL_Y, ((mStat & UP)    ? -1 : 0) );       // Move Up
        emit(fd, EV_REL, REL_X, ((mStat & RIGHT) ?  1 : 0) );       // Move Right
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    if( (mStat & DOWN) || (mStat & LEFT) )
    {
        emit(fd, EV_REL, REL_Y, ((mStat & DOWN) ?  1 : 0) );     // Move Down
        emit(fd, EV_REL, REL_X, ((mStat & LEFT) ? -1 : 0) );     // Move Left
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    if( (btnStat & RIGHT_BUTTON) != (mStat & RIGHT_BUTTON) )
    {
        emit(fd, EV_MSC, MSC_SCAN, 0x100);      // Left mouse button scan code
        emit(fd, EV_KEY, BTN_RIGHT, ((mStat & RIGHT_BUTTON) ? 1 : 0)); // Right Button Press or relase
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    if( (btnStat & LEFT_BUTTON) != (mStat & LEFT_BUTTON) )
    {
        emit(fd, EV_MSC, MSC_SCAN, 0x101);      // Right mouse button scan code
        emit(fd, EV_KEY, BTN_LEFT, ((mStat & LEFT_BUTTON) ? 1 : 0));   // Left Button Press or relase
        emit(fd, EV_SYN, SYN_REPORT, 0);
    }

    if((pEvent->code == cfg.mbc.scrollUp || pEvent->code == cfg.mbc.scrollDown) && pEvent->value)
    {

        if(
            (mStat & GEAR_UP_BUTTON) ||
            ((btnStat & GEAR_UP_BUTTON) != (mStat & GEAR_UP_BUTTON))
          )
        {
            emit(fd, EV_MSC, MSC_SCAN, 0x108);      // Gear up scan code
            emit(fd, EV_REL, REL_WHEEL, 1);  // Mouse wheel gear up
            emit(fd, EV_SYN, SYN_REPORT, 0);
        }

        if(
            (mStat & GEAR_DOWN_BUTTON) ||
            ((btnStat & GEAR_DOWN_BUTTON) != (mStat & GEAR_DOWN_BUTTON))
         )
        {
            emit(fd, EV_MSC, MSC_SCAN, 0x109);      // Gear down scan code
            emit(fd, EV_REL, REL_WHEEL, -1);  // Mouse wheel gear down
            emit(fd, EV_SYN, SYN_REPORT, 0);
        }
    }
    btnStat = mStat;

}

// Name                 : mouse_speed
// Parameters           : pEvent        > hold Keyboard key event that assigned to the virtual mouse
// Call                 : sin()
// Called by            : main()
// Return               : Updated mouse speed
// Description          : Control the speed and the acceleration of the mouse pointer

int mouse_speed(struct input_event *pEvent)
{
    static int speed = 10000;
    static int tmpSpeed = 0xffff;

    static int i = 0;
    float k = (cfg.msc.minSpeed - cfg.msc.maxSpeed)/2.0f;
    static int keysTracker = 0;         // Count how many keys currently pressed

   if(
       pEvent->type == EV_KEY &&
       (
           (pEvent->code == cfg.mbc.up)    ||
           (pEvent->code == cfg.mbc.down)  ||
           (pEvent->code == cfg.mbc.right) ||
           (pEvent->code == cfg.mbc.left)
       )
    )
   {
        if(pEvent->value == 1)
            ++keysTracker;

        if(!pEvent->value)
            --keysTracker;

       if(pEvent->value || keysTracker)
       {
            if(speed < tmpSpeed)
            {
                tmpSpeed = speed;
                // Simple Sine function for mouse acceleration control
                speed = k*sin(i/cfg.msc.acc + 1.5708f) + k + cfg.msc.maxSpeed;
                ++i;
            }
            else
                speed = tmpSpeed;
      }
       else
       {
            speed = cfg.msc.minSpeed;
            tmpSpeed = 0xffff;
            i = 0;
       }
   }

    return speed;
}

// Name                 : wait_keys_release
// Parameters           : KBfd          > Keyboard event file descriptor
// Call                 : memset(), ioctl()
// Called by            : write_KBevent(), mouse_config()
// Return               : void
// Description          : Wait till all keyboard keys are released

void wait_keys_release(int KBfd)
{
    int arraySize = KEY_MAX/8 + 1;      // divide the KEY_MAX by byte
    char keyBit[arraySize];

    int i = 0;

    for(;;)
    {
        memset(keyBit, 0, arraySize);
        ioctl(KBfd, EVIOCGKEY(arraySize), keyBit);

        for( ; i < arraySize ; ++i)
        {
            if(keyBit[i])
                break;
        }
        if(i < arraySize)
        {
            i = 0;
            continue;
        }

        break;
    }

}

// Name                 : mouse_config
// Parameters           : cmdArg        > Program command line argument
//                        kbPath        > Full Keyboard event file path
// Call                 : sizeof(), open(), default_config(), read(), write(), close()
//                        remove(), strcmp(), fprintf(), exit(), clear_screen()
//                        printf(), key_id(), scanf(), getchar(), terminal_echo()
//                        wait_keys_release(), flush_KB(), sleep()
// Called by            : main()
// Return               : void
// Description          : Assign 8 keys (default or custom) from the keyboard to control the virtual mouse device
//                        and set the max, min and acceleration values to control the mouse pointer speed
//                        if mouse.cfg file not exist then the program starts with the default configuration
void mouse_config(char *cmdArg, char *kbPath)
{
    int KBfd, MCfd;
    char choice = 'Y';
    const char KEYCHUNCK = 3 * sizeof(struct input_event);
    struct input_event keyEvent[3];
    char key[MAX_KEY];
    int maxSpeedMAX = 0,
        maxSpeedMIN = 3000,
        minSpeedMAX = 10000,
        minSpeedMIN = 1000;

    // Configuration file doesn't exist .. go with the default configuration
    if((MCfd = open("mouse.cfg", O_RDONLY)) < 0)
        default_config();
    else
    {
        if(read(MCfd, &cfg, sizeof(cfg)) == sizeof(cfg))
            close(MCfd);
        else
        {
            remove("mouse.cfg");
            default_config();
        }
    }

    if(
        cmdArg &&
        (
            (strcmp("--config", cmdArg) == 0) ||
            (strcmp("-C", cmdArg) == 0)
        )
     )
    {

        if((KBfd = open(kbPath, O_RDONLY)) < 0)
        {
            fprintf(stderr, "Couldn't open \"%s\" file for reading\n", kbPath);
            exit(-1);
        }

        do
        {
           clear_screen();

            printf("MOUSE DIRECTIONS:\n");
            printf("----------------\n\n");
            key_id(cfg.mbc.up, key);
            printf("01- Up movement key   (0x%x) %s\n", cfg.mbc.up, key);
            key_id(cfg.mbc.down, key);
            printf("02- Down movement key (0x%x) %s\n", cfg.mbc.down, key);
            key_id(cfg.mbc.right, key);
            printf("03- Right movment key (0x%x) %s\n", cfg.mbc.right, key);
            key_id(cfg.mbc.left, key);
            printf("04- Left movement key (0x%x) %s\n\n", cfg.mbc.left, key);

            printf("MOUSE BUTTONS:\n");
            printf("--------------\n\n");
            key_id(cfg.mbc.rBtn, key);
            printf("05- Right button key  (0x%x) %s\n", cfg.mbc.rBtn, key);
            key_id(cfg.mbc.lBtn, key);
            printf("06- Left button key   (0x%x) %s\n", cfg.mbc.lBtn, key);
            key_id(cfg.mbc.scrollUp, key);
            printf("07- Wheel Up key      (0x%x) %s\n", cfg.mbc.scrollUp, key);
            key_id(cfg.mbc.scrollDown, key);
            printf("08- Wheel Down key    (0x%x) %s\n\n", cfg.mbc.scrollDown, key);

            printf("MOUSE SPEED:\n");
            printf("-----------\n\n");
            printf("09- Max speed     (%d - %d)     ----> [%d]\n", maxSpeedMAX, maxSpeedMIN, cfg.msc.maxSpeed);
            printf("10- Minimum speed (%d - %d)     ----> [%d]\n", minSpeedMIN, minSpeedMAX, cfg.msc.minSpeed);
            printf("11- Acceleration  ( > 0.00)     ----> [%.2f]\n\n", cfg.msc.acc);

            printf("00- Quit\n\n\n");
            printf("Choose the line number of the key/value want to change (0-11): ");
            scanf("%hhd", &choice);
            getchar();

            terminal_echo(0);   // Disable terminal echo

            wait_keys_release(KBfd);
            KBfd = flush_KB(KBfd, kbPath);

            printf("\n");
            switch(choice)
            {
                case 1:
                    printf("Press the new UP movement key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.ups = keyEvent[0].value;  // key scan code
                    cfg.mbc.up  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.up, key);
                    break;
                case 2:
                    printf("Press the new DOWN movement key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.downs = keyEvent[0].value;  // key scan code
                    cfg.mbc.down  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.down, key);
                    break;
                case 3:
                    printf("Press the new RIGHT movement key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.rights = keyEvent[0].value;  // key scan code
                    cfg.mbc.right  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.right, key);
                    break;
                case 4:
                    printf("Press the new LEFT movement key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.lefts = keyEvent[0].value;  // key scan code
                    cfg.mbc.left  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.left, key);
                    break;
                case 5:
                    printf("Press the new RIGHT BUTTON key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.rBtns = keyEvent[0].value;  // key scan code
                    cfg.mbc.rBtn  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.rBtn, key);
                    break;
                case 6:
                    printf("Press the new LEFT BUTTON key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.lBtns = keyEvent[0].value;  // key scan code
                    cfg.mbc.lBtn  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.lBtn, key);
                    break;
                case 7:
                    printf("Press the new WHEEL UP (SCROLL UP) key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK);
                    cfg.mbc.scrollUps = keyEvent[0].value;  // key scan code
                    cfg.mbc.scrollUp  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.scrollUp, key);
                    break;
                case 8:
                    printf("Press the new WHEEL DOWN (SCROLL DOWN) key ...\n\n");
                    read(KBfd, &keyEvent, KEYCHUNCK); cfg.mbc.scrollDowns = keyEvent[0].value;  // key scan code
                    cfg.mbc.scrollDown  = keyEvent[1].code;   // key code
                    cfg.mbc.scrollDowns = keyEvent[0].value;  // key scan code
                    cfg.mbc.scrollDown  = keyEvent[1].code;   // key code
                    key_id(cfg.mbc.scrollDown, key);
                    break;
                case 9:
                    terminal_echo(1);
                    printf("Enter the new MAX SPEED value : ");
                    scanf("%d", &cfg.msc.maxSpeed);
                    getchar();
                    if(cfg.msc.maxSpeed < maxSpeedMAX)
                        cfg.msc.maxSpeed = maxSpeedMAX;
                    else if(cfg.msc.maxSpeed > maxSpeedMIN)
                        cfg.msc.maxSpeed = maxSpeedMIN;
                    break;
                case 10:
                    terminal_echo(1);
                    printf("Enter the new MIN SPEED value : ");
                    scanf("%d", &cfg.msc.minSpeed);
                    getchar();
                    if(cfg.msc.minSpeed > minSpeedMAX)
                        cfg.msc.minSpeed = minSpeedMAX;
                    else if(cfg.msc.minSpeed < minSpeedMIN)
                        cfg.msc.minSpeed = minSpeedMIN;
                    break;
                case 11:
                    terminal_echo(1);
                    printf("Enter the new ACCELERATION value : ");
                    scanf("%f", &cfg.msc.acc);
                    getchar();
                    if(cfg.msc.acc < 0)
                        cfg.msc.acc = 1;
                    break;
            }

            if((MCfd = open("mouse.cfg", O_WRONLY | O_CREAT, 0644)) < 0)
            {
                fprintf(stderr, "Couldn't open mouse.cfg file for writing.\n");
                exit(-1);
            }

            write(MCfd, &cfg, sizeof(cfg));
            close(MCfd);
            wait_keys_release(KBfd);
            KBfd = flush_KB(KBfd, kbPath);
            terminal_echo(1);

        }while(choice);

        clear_screen();
        sleep(1);
        wait_keys_release(KBfd);
        close(KBfd);

        terminal_echo(0);
    }

}

// Name                 : default_config
// Parameters           : void
// Call                 : void
// Called by            : mouse_config()
// Return               : void
// Description          : Assign default 8 keyboard keys to control virtual mouse:
//                        - Up arrow key to move the mouse pointer up, down arrow key to move down, ... and so on
//                        - O keypad key for mouse left button and ENTER keypad key for right button
//                        - 8 keypad key to scroll up (wheel up) and 2 keypad key to scroll down (wheel down)

void default_config(void)
{
    cfg.msc.maxSpeed = 700;
    cfg.msc.minSpeed = 10000;
    cfg.msc.acc = 2.5;

    // Key Codes
    cfg.mbc.up = 0x67;       // Up arrow
    cfg.mbc.down = 0x6c;     // Down arrow
    cfg.mbc.right = 0x6a;    // Right arrow
    cfg.mbc.left = 0x69;     // Left arrow

    cfg.mbc.rBtn = 0x60;         // "Enter" keypad
    cfg.mbc.lBtn = 0x52;         // "0" Keypad
    cfg.mbc.scrollUp = 0x48;     // "8" Keypad
    cfg.mbc.scrollDown = 0x50;   // "2" Keypad

    // Scan Codes
    cfg.mbc.ups = 0xc8;       // Up arrow
    cfg.mbc.downs = 0xd0;     // Down arrow
    cfg.mbc.rights = 0xcd;    // Right arrow
    cfg.mbc.lefts = 0xcb;     // Left arrow

    cfg.mbc.rBtns = 0x9c;         // "Enter" keypad
    cfg.mbc.lBtns = 0x52;         // "0" Keypad
    cfg.mbc.scrollUps = 0x48;     // "8" Keypad
    cfg.mbc.scrollDowns = 0x50;   // "2" Keypad
}


// Name                 : terminal_echo
// Parameters           : echoStatus    >      Flag to control terminal echo
// Call                 : tcgetattr(), tcsetattr(), tcflush()
// Called by            : mouse_config()
// Return               : void
// Description          : Control the terminal echo using echoStatus as a switch
//                        that discard unwritten buffer when echStatus > 0

void terminal_echo(int echoStatus)
{
    struct termios attr;
    tcgetattr(0, &attr);

    if(echoStatus)
    {
        tcflush(0, TCIFLUSH);
        attr.c_lflag |= ECHO;
    }
    else
        attr.c_lflag &= ~ECHO;

    tcsetattr(0, TCSANOW, &attr);
}

// Name             : flush_KB
// Parameters       : KBfd          > Keyboard event file descriptor
//                    kbPath        > Full Keyboard event file path
// Call             : close(), open(), fprintf(), exit()
// Called by        : mouse_config()
// Return           : Keyboard event file descriptor
// Description      : Discard all the keys that not yet read from the keyboard event file by closing and opening the file

int flush_KB(int KBfd, char *kbPath)
{
    int fd;
    close(KBfd);

    if((fd = open(kbPath, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Couldn't open \"%s\" file for reading\n", kbPath);
        exit(-1);
    }

    return fd;
}

// Name             : key_id
// Parameters       : code          > Numberic keycode
//                    key           > The text id of the keycode
// Call             : strcpy()
// Called by        : mouse_config()
// Return           : void
// Description      : Convert the numeric keycode to its text id

void key_id(int code, char *key)
{
    switch(code)
    {
        case 1 : strcpy(key, "ESC"); break;
        case 2 : strcpy(key, "1"); break;
        case 3 : strcpy(key, "2"); break;
        case 4 : strcpy(key, "3"); break;
        case 5 : strcpy(key, "4"); break;
        case 6 : strcpy(key, "5"); break;
        case 7 : strcpy(key, "6"); break;
        case 8 : strcpy(key, "7"); break;
        case 9 : strcpy(key, "8"); break;
        case 10 : strcpy(key, "9"); break;
        case 11 : strcpy(key, "0"); break;
        case 12 : strcpy(key, "- (MINUS)"); break;
        case 13 : strcpy(key, "= (EQUAL)"); break;
        case 14 : strcpy(key, "BACKSPACE"); break;
        case 15 : strcpy(key, "TAB"); break;
        case 16 : strcpy(key, "Q"); break;
        case 17 : strcpy(key, "W"); break;
        case 18 : strcpy(key, "E"); break;
        case 19 : strcpy(key, "R"); break;
        case 20 : strcpy(key, "T"); break;
        case 21 : strcpy(key, "Y"); break;
        case 22 : strcpy(key, "U"); break;
        case 23 : strcpy(key, "I"); break;
        case 24 : strcpy(key, "O"); break;
        case 25 : strcpy(key, "P"); break;
        case 26 : strcpy(key, "[ (LEFTBRACE)"); break;
        case 27 : strcpy(key, "] (RIGHTBRACE)"); break;
        case 28 : strcpy(key, "ENTER"); break;
        case 29 : strcpy(key, "LEFTCTRL"); break;
        case 30 : strcpy(key, "A"); break;
        case 31 : strcpy(key, "S"); break;
        case 32 : strcpy(key, "D"); break;
        case 33 : strcpy(key, "F"); break;
        case 34 : strcpy(key, "G"); break;
        case 35 : strcpy(key, "H"); break;
        case 36 : strcpy(key, "J"); break;
        case 37 : strcpy(key, "K"); break;
        case 38 : strcpy(key, "L"); break;
        case 39 : strcpy(key, ";"); break;
        case 40 : strcpy(key, "' (APOSTROPHE)"); break;
        case 41 : strcpy(key, "` (GRAVE)"); break;
        case 42 : strcpy(key, "LEFTSHIFT"); break;
        case 43 : strcpy(key, "\\ (BACKSLASH)"); break;
        case 44 : strcpy(key, "Z"); break;
        case 45 : strcpy(key, "X"); break;
        case 46 : strcpy(key, "C"); break;
        case 47 : strcpy(key, "V"); break;
        case 48 : strcpy(key, "B"); break;
        case 49 : strcpy(key, "N"); break;
        case 50 : strcpy(key, "M"); break;
        case 51 : strcpy(key, ", (COMMA)"); break;
        case 52 : strcpy(key, ". (DOT)"); break;
        case 53 : strcpy(key, "/ (SLASH)"); break;
        case 54 : strcpy(key, "RIGHTSHIFT"); break;
        case 55 : strcpy(key, "* (KPASTERISK)"); break; // KP stand for keypad
        case 56 : strcpy(key, "LEFTALT"); break;
        case 57 : strcpy(key, "SPACE"); break;
        case 58 : strcpy(key, "CAPSLOCK"); break;
        case 59 : strcpy(key, "F1"); break;
        case 60 : strcpy(key, "F2"); break;
        case 61 : strcpy(key, "F3"); break;
        case 62 : strcpy(key, "F4"); break;
        case 63 : strcpy(key, "F5"); break;
        case 64 : strcpy(key, "F6"); break;
        case 65 : strcpy(key, "F7"); break;
        case 66 : strcpy(key, "F8"); break;
        case 67 : strcpy(key, "F9"); break;
        case 68 : strcpy(key, "F10"); break;
        case 69 : strcpy(key, "NUMLOCK"); break;
        case 70 : strcpy(key, "SCROLLLOCK"); break;
        case 71 : strcpy(key, "KP7"); break;
        case 72 : strcpy(key, "KP8"); break;
        case 73 : strcpy(key, "KP9"); break;
        case 74 : strcpy(key, "- (KPMINUS)"); break;
        case 75 : strcpy(key, "KP4"); break;
        case 76 : strcpy(key, "KP5"); break;
        case 77 : strcpy(key, "KP6"); break;
        case 78 : strcpy(key, "+ (KPPLUS)"); break;
        case 79 : strcpy(key, "KP1"); break;
        case 80 : strcpy(key, "KP2"); break;
        case 81 : strcpy(key, "KP3"); break;
        case 82 : strcpy(key, "KP0"); break;
        case 83 : strcpy(key, ". (KPDOT)"); break;
        case 86 : strcpy(key, "102ND"); break;
        case 87 : strcpy(key, "F11"); break;
        case 88 : strcpy(key, "F12"); break;
        case 96 : strcpy(key, "KPENTER"); break;
        case 97 : strcpy(key, "RIGHTCTRL"); break;
        case 98 : strcpy(key, "KPSLASH"); break;
        case 99 : strcpy(key, "SYSRQ"); break;
        case 100 : strcpy(key, "RIGHTALT"); break;
        case 102 : strcpy(key, "HOME"); break;
        case 103 : strcpy(key, "UP"); break;
        case 104 : strcpy(key, "PAGEUP"); break;
        case 105 : strcpy(key, "LEFT"); break;
        case 106 : strcpy(key, "RIGHT"); break;
        case 107 : strcpy(key, "END"); break;
        case 108 : strcpy(key, "DOWN"); break;
        case 109 : strcpy(key, "PAGEDOWN"); break;
        case 110 : strcpy(key, "INSERT"); break;
        case 111 : strcpy(key, "Dflush_KBELETE"); break;
        case 117 : strcpy(key, "= (KPEQUAL)"); break;
        case 118 : strcpy(key, "KPPLUSMINUS"); break;
        case 125 : strcpy(key, "LEFTMETA"); break;
        case 126 : strcpy(key, "RIGHTMETA"); break;
        case 127 : strcpy(key, "COMPOSE"); break;
        default : strcpy(key, "UNKNOWN"); break;
    }
}

// Name             : clear_screen
// Parameters       : void
// Call             : system()
// Called by        : mouse_config()
// Return           : void
// Description      : Clear the terminal screen

void clear_screen(void)
{
    system("clear");
}

// Name             : write_KBevent (Thread function)
// Parameters       : kbPath        > Full Keyboard event file path
// Call             : sizeof(), create_keyboard_device(), fprintf(), pthread_exit(),
//                    open(), wait_keys_release(), ioctl(), perror(), exit(), read(),
//                    write(), printf(), close()
// Called by        : main() *as a thread*
// Return           : void
// Description      : Grap every key event from the original keyboard device if key event is one of the "keyboard mouse keys"
//                    then let it to be processed by main() else write the key event to the virtual keyboard

void *write_KBevent(void *kbPath)
{
    int vKBfd,
         KBfd;

    const int CHUNCK = sizeof(struct input_event);
    struct input_event ev = { .type = 0xff, .code = 0xff, .value = 0xff};
    struct input_event ie;

    if((vKBfd = create_keyboard_device()) < 0)
    {
        fprintf(stderr, "Couldn't create mouse virtual device.\n");
        pthread_exit(NULL);
    }

    if((KBfd = open((char *)kbPath, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Couldn't open %s file for reading.\n", (char *)kbPath);
        pthread_exit(NULL);
    }

    // Wait for the relase of all keys
    wait_keys_release(KBfd);

    // Grab keyboard device
    if(ioctl(KBfd, EVIOCGRAB, 1) < 0)
    {
         perror("evdev ioctl -> EVIOCGRAB ");
         exit(1);
    }


    for( ; (ie.code != KEY_ESC) || ie.value ; )
    {
        read(KBfd, &ie, CHUNCK);

        if(ie.type == EV_MSC)
        {
            if(
                (ie.value == cfg.mbc.ups)         ||
                (ie.value == cfg.mbc.downs)       ||
                (ie.value == cfg.mbc.rights)      ||
                (ie.value == cfg.mbc.lefts)       ||
                (ie.value == cfg.mbc.rBtns)       ||
                (ie.value == cfg.mbc.lBtns)       ||
                (ie.value == cfg.mbc.scrollUps)   ||
                (ie.value == cfg.mbc.scrollDowns)
              )
              {
                  ev.type = EV_MSC;
                  es.events[es.cWrite] = ie;
                  es.cWrite = ++es.cWrite % EVENTS_BUFFER;
                  ++es.diff;
                  continue;
              }
        }
        else if( (ev.type == EV_MSC) && (ie.type == EV_KEY) )
        {
            ev.type = EV_KEY;
            es.events[es.cWrite] = ie;
            es.cWrite = ++es.cWrite % EVENTS_BUFFER;
            ++es.diff;
            continue;
        }
        else if( (ev.type == EV_KEY) && (ie.type == EV_SYN) )
        {
            ev.type = EV_SYN;
            es.events[es.cWrite] = ie;
            es.cWrite = ++es.cWrite % EVENTS_BUFFER;
            ++es.diff;
            continue;
        }

        write(vKBfd, &ie, CHUNCK);
    }

    // read synchronize event
    read(KBfd, &ie, CHUNCK);
    write(vKBfd, &ie, CHUNCK);

    es.exitFlag = 1;

    // Ungrab keyboard device
    if(ioctl(KBfd, EVIOCGRAB, 0) < 0)
    {
         perror("evdev ioctl -> EVIOCGRAB ");
         exit(1);
    }

    ioctl(vKBfd, UI_DEV_DESTROY);
    close(vKBfd);

}

// Name             : help
// Parameters       : cmdArg        > program name from command line argument
// Call             : printf(), exit()
// Called by        : main()
// Return           : void
// Description      : simple help message to use the program
void help(char *cmdArg)
{
    printf("\nUsage : sudo %s [options]\n\n", cmdArg);
    printf("\n -C, --config          Configure mouse\n");
    printf(" -h, --help            help message\n\n");
    exit(0);
}
