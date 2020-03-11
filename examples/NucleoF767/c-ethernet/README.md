
# Example Project: C-Ethernet

## Goal

Demonstrate Ethernet connectivity with Nucleo-F767 boards, by transmitting raw
Ethernet II frames and printing received frames over debug-serial.

## Brief

The example makes use of the low-level Ethernet driver that's part of
Essentials/MCU to control the on-chip Ethernet-MAC provided by the STM32F7xx
chip-family.

On-board LEDs are used to signal transmit and receive status. The
 <span style="color:blue">blue</span> LED is
used to indicate the transmit of a Ethernet frame.
 <span style="color:green">Green</span> is used to indicate
incoming frames. Whenever indicated, the LEDs will stay on for 100 milliseconds
to be visible. Of course, frames might be sent with higher periodicity than
that. <span style="color:red">Red</span> indicates some error during Ethernet
communication. It will reset after 500 milliseconds if no further errors are
registered.

**Note:** On boot, all LEDs will light up once according to their indication
delay (i.e. <span style="color:blue">blue</span> and
<span style="color:green">green</span> 100ms,
<span style="color:red">red</span>: 500ms). This is just to indicate to the user
that the app booted successfully.

### Components Required

#### Software Components

- Binary compiled from Kiso example files
- Wireshark running on connected PC
- Python installed on connected PC
  - Locate the `send_frame.py` script in the ethernet example folder.
  - Version >=3.6 required.
  - Please install the python modules listed in `requirements.txt`. You can do
    so by running \
    `pip install -r requirements.txt`.

#### Hardware Components

- STM32F767 Nucleo-144 dev-kit with populated RJ-45 port
- Ethernet cable Cat. 5, Cat. 6, etc (any of the shelf Ethernet branded cable
  will do)
- Computer connected to the dev-kit
  - via Ethernet to send and receive frames
  - via USB COM-port to monitor debug messages about received frames

## (*Optional*) Initial Setup TODO

Any hardware or software(environment) setup that needs to be done prior to execution/usage of the current example.

## (*Optional*) Execution Flow TODO

For certain examples, perceived to be complex, a figurative projection of the execution flow will serve wonders in adapting the example by future developers.
*This section is optional as for very basic examples this might be a bit too much effort.*

## (*Optional*)Code Walkthrough TODO

This section is inspired by this [project](https://github.com/eclipse/iceoryx/tree/master/iceoryx_examples/icedelivery). It describes some peculiarities in the code structure of the application like includes, datatypes(structs and what not), some key processing steps, some important condition checks etc.

*Experienced developers of Kiso are requested to evaluate what they want to emphasize here from the implementation.*

## Expected Output TODO

Specify what the user should obtain as an output of the execution the example. e.g. terminal output screenshots, pictures(flashing LEDs) etc.

## Troubleshooting TODO

What can go wrong(most often) and how to solve the issue. Optionally a contact if not solvable by the user.
