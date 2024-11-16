

#include <fcntl.h>
#include <unistd.h>
#include <linux/uinput.h>
#include <cstring>
#include <iostream>
#include <libinput.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// why does developing random shit on linux so messy. to get key state on windows just use getkeystate and its easy to find in the winapi docs compared to linux docs.
// mabye im just not used to it. i dont want to download a zillion librarys to do simple shit



static bool hold = false; // meh. way to tell that ctrl is being held down.
class MouseEmulation
{
public:
  int uinputshit;
  int click_delay;
  MouseEmulation(int click_delayms): click_delay{click_delayms}, uinputshit{0}
  {
      initUinput();
  }

  ~MouseEmulation()
  {
    sleep(1);

    ioctl(uinputshit, UI_DEV_DESTROY);
    close(uinputshit);
  }

  void emit(int fd, int type, int code, int val) {
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    ie.time.tv_sec = 0;
    ie.time.tv_usec = 0;

    ssize_t i =write(fd, &ie, sizeof(ie));
  }

  int initUinput()
  {
    uinputshit = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uinputshit < 0)
    {
      std::cerr << "Failed to open /dev/uinput\n";
      return -1;
    }


    /*
     * The ioctls below will enable the device that is about to be
     * created, to pass key events, in this case the space key.
     */
    ioctl(uinputshit, UI_SET_EVBIT, EV_KEY);
    ioctl(uinputshit, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(uinputshit, UI_SET_EVBIT, EV_REL);
    ioctl(uinputshit, UI_SET_RELBIT, REL_X);
    ioctl(uinputshit, UI_SET_RELBIT, REL_Y);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "Virtual Mouse");

    ioctl(uinputshit, UI_DEV_SETUP, &usetup);
    ioctl(uinputshit, UI_DEV_CREATE);
    /*
        * On UI_DEV_CREATE the kernel will create the device node for this
        * device. We are inserting a pause here so that userspace has time
        * to detect, initialize the new device, and can start listening to
        * the event, otherwise it will not notice the event we are about
        * to send. This pause is only needed in our example code!
        */


    sleep(1);
    return 1;
  }



  void sendLeftClick()
  {

    emit(uinputshit, EV_KEY, BTN_LEFT, 1);
    emit(uinputshit, EV_SYN, SYN_REPORT, 0);
    usleep(100000);
    emit(uinputshit, EV_KEY, BTN_LEFT, 0);
    emit(uinputshit, EV_SYN, SYN_REPORT, 0);
    /*SYN_REPORT
     *Used to synchronize and separate events into packets of input data changes occurring at the same moment in time.
     *For example, motion of a mouse may set the REL_X and REL_Y values for one motion, then emit a SYN_REPORT.
     *The next motion will emit more REL_X and REL_Y values and send another SYN_REPORT.
     */
    usleep(click_delay * 1000);

  }
};

void process_event(libinput_event *event) {
  if (libinput_event_get_type(event) == LIBINPUT_EVENT_KEYBOARD_KEY) {

    struct libinput_event_keyboard *key_event = libinput_event_get_keyboard_event(event);

    uint32_t key = libinput_event_keyboard_get_key(key_event);
    libinput_key_state state = libinput_event_keyboard_get_key_state(key_event);

    std::cout << "Key: " << key << " State: " << (state == LIBINPUT_KEY_STATE_PRESSED ? "PRESSEDDDDDD" : "RELEASEDDDD") << std::endl;

    if(key == 29 && state == LIBINPUT_KEY_STATE_PRESSED)
    {
      hold = true;
    }
    else if (key == 29 && state == LIBINPUT_KEY_STATE_RELEASED){
      hold = false;
    }

  }
}

static int libinput_open_restricted(const char *path, int flags, void *data) {
  int fd = open(path, flags);
  if (fd <= 0) {
    std::cout << "Couldnt open path to DEVICE" << std::endl;
    return -errno;
  }

  return fd;
}

static void libinput_close_restricted(int fd, void *data) {

  if (close(fd) != 0) {
  }
}

static const struct libinput_interface libinput_impl = {
  .open_restricted = libinput_open_restricted,
  .close_restricted = libinput_close_restricted
};

int main(int argc, char *argv[]){

  if (argc != 2)
  {
    std::cerr <<  "Please include a device path for uinput from /dev/input/by-id/fdjog-kbd" << std::endl;
    return 0;
  }

  MouseEmulation mouse(500); // miliseconds


  // go to ls /dev/input/by-id/
  // and find the keyboard

  char device_path[1024];

  strncpy(device_path, argv[1], 1024); // no strcpy_s so here we are

  //const char *device_path ;//"/dev/input/by-id/usb-Microsoft_Surface_Type_Cover-event-kbd"; // gotta adjust thisss
  //int fd = open(device_path, O_RDONLY | O_NONBLOCK); // not needed because when gets added to libinput via  libinput_path_add_device
  //if (fd < 0) {
  //  std::cerr << "failed to open device: " << strerror(errno) << std::endl;
  //  return -1;
  //}
  std::cout << device_path << std::endl;
  struct libinput *li = libinput_path_create_context(&libinput_impl, nullptr);
  if (!li) {
    std::cerr << "failed to create libinput context" << std::endl;
    return -1;
  }

  struct libinput_device *device = libinput_path_add_device(li, device_path);
  if (!device) {
    std::cerr << "failed to add device to libinput context" << std::endl;
    libinput_unref(li);
    return -1;
  }

  std::cout << "monitoring key presses on: " << device_path << std::endl;

  while (true) {
    libinput_dispatch(li);
    libinput_event *event;
    while ((event = libinput_get_event(li))) {
      process_event(event);
      libinput_event_destroy(event);
    }
    if (hold)
      mouse.sendLeftClick();
  }

  // TODO should rly have a key that escapes the loop to clean stuff properly
  libinput_unref(li);
  return 0;
}

