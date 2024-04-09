// Example usage for DFRobot_PN532 library by derrickrc.

#include "DFRobot_PN532.h"

// Initialize objects from the lib
DFRobot_PN532 dFRobot_PN532;

void setup() {
    // Call functions on initialized library objects that require hardware
    dFRobot_PN532.begin();
}

void loop() {
    // Use the library's initialized objects and functions
    dFRobot_PN532.process();
}
