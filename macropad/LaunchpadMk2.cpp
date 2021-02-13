#include <windows.h>
#include <array>
#include <sstream>
#include <iomanip>
#include "framework.h"
#include "LaunchpadMk2.h"
#include "macropad.h"
#include "Config.h"

bool midi_device::launchpadmk2::execute_all = true;

void midi_device::launchpadmk2::LaunchpadMk2::Init()
{
	unsigned int nPorts = in->getPortCount();
    std::string portName;
    for (unsigned int i = 0; i < nPorts; i++) {
        try {
            portName = in->getPortName(i);
        }
        catch (RtMidiError& error) {
            error.printMessage();
        }
        _DebugString("  Input Port #" + std::to_string(i) + ": " + portName + "\n");
        
        if (portName.find("Launchpad MK2") != std::string::npos) {
            _DebugString("Using input port " + std::to_string(i) + ".\n");
            try {
                in->openPort(i);
            }
            catch (RtMidiError& error) {
                _DebugString("Failed to use input port.\n");
            }
            break;
        }
    }

    // Don't ignore sysex, timing, or active sensing messages.
    in->ignoreTypes(false, false, false);

    nPorts = out->getPortCount();
    _DebugString("There are " + std::to_string(nPorts) + " MIDI output sources available.\n");
    for (unsigned int i = 0; i < nPorts; i++) {
        try {
            portName = out->getPortName(i);
        }
        catch (RtMidiError& error) {
            error.printMessage();
        }
        _DebugString("  Output Port #" + std::to_string(i) + ": " + portName + "\n");

        if (portName.find("Launchpad MK2") != std::string::npos) {
            _DebugString("Using output port " + std::to_string(i) + ".\n");
            try {
                out->openPort(i);
            }
            catch (RtMidiError& error) {
                _DebugString("Failed to use output port.\n");
            }
            break;
        }
    }

    this->setup_pages();
    this->fullLedUpdate();
}

void midi_device::launchpadmk2::LaunchpadMk2::reset()
{
	// does not exist
    return;
}

void midi_device::launchpadmk2::LaunchpadMk2::RunDevice()
{
    main_device = new LaunchpadMk2();
    main_device->Init();
    main_device->Loop();
}

/// Input loop
void midi_device::launchpadmk2::LaunchpadMk2::Loop()
{
    std::vector<unsigned char> message;
    int nBytes, i;
    double stamp;
    config::ButtonBase* button;

	while (should_loop && execute_all)
	{
        stamp = in->getMessage(&message);
        nBytes = message.size();

        // no message. skip
        if (nBytes == 0) {
            continue;
        }

        for (i = 0; i < nBytes; i++)
            _DebugString("Byte " + std::to_string(i) + " = " + std::to_string((int)message[i]) + ", ");
        if (nBytes > 0)
            _DebugString("stamp = " + std::to_string(stamp) + "\n");

		if (nBytes == 3)
		{
            // grid button pressed or released.
            if (message[0] == 0x90) {
                // we want to only handle inside grid buttons, not the "page" side buttons.
                if (message[1] % 0x10 <= 0x07) {
                    button = get_button(message[1]);

                    // pressed
                    if (message[2] == 0x7F) {
                        this->sendMessage(commands::led_set(message[1],
                            5));
                    }
                    // released.
                    else if (message[2] == 0x00) {
                        if (button == nullptr) {
                            this->sendMessage(commands::led_off(message[1]));
                        }
                        else {
                            button->execute();

                            this->sendMessage(commands::led_set(message[1],
                                button->get_color()));
                        }
                    }
                }
                else {
                    // pressed
                    if (message[2] == 0x7F) {
                        // change page.
                        page = message[1] / 0x10;

                        // update all buttons
                        this->fullLedUpdate();
                    }
                    // released.
                    else if (message[2] == 0x00) {
                    }
                }

            }
            // Automap/Live buttons pressed or released.
            /* else if (message[0] == 0xB0) {
                // pressed
                if (message[2] == 0x7F) {
                    if (message[1] >= 108)
                        mode = static_cast<launchpad::mode>(message[1]);

                    this->fullLedUpdate();
                }
                // released.
                else if (message[2] == 0x00) {

                }
            } */
		}
        button = nullptr;
        message.clear();
	}

    // end of loop. reset
    this->reset();
}

midi_device::launchpadmk2::config::ButtonBase* midi_device::launchpadmk2::LaunchpadMk2::get_button(unsigned char key)
{
    if (page >= pages.size()) {
        return nullptr;
    }

    if (pages.at(page) == nullptr) {
        return nullptr;
    }

    int x, y;
    commands::calculate_xy_from_keycode(key, x, y);

    return pages.at(page)->at(x).at(y);
}

// custom calculated messages go here
void midi_device::launchpadmk2::LaunchpadMk2::sendMessage(unsigned char* message)
{
    if (!out->isPortOpen()) {
        goto cleanup;
    }

    try {
        out->sendMessage(message, sizeof(unsigned char) * 3);
    }
    catch (RtMidiError& error) {
        error.printMessage();
        _DebugString(error.getMessage());
    }

cleanup:
    delete[] message;
}

// this will literally update EVERYTHING. do NOT call this function unless you ABSOLUTELY NEED TO!
void midi_device::launchpadmk2::LaunchpadMk2::fullLedUpdate()
{
    // don't do anything.
    if (!out->isPortOpen())
        return;

    // reset everything first.
    out->sendMessage(commands::reset, sizeof(unsigned char) * 3);

    // set our page indicator to color 12, yellow
	// REFER TO THE MK2 PROGRAMMER'S MANUAL!!!! i should probably do this programatically
    this->sendMessage(commands::led_setPalette(
        0x10 * page + 0x08,
        12));

    // set our "mode" indicator
    this->sendMessage(new unsigned char[3]{ 0xB0, (unsigned char)mode,
        12 });

    // update every LEDs.
    if ((size_t)page < pages.size()) {

        // row
        for (size_t row = 0; row < pages.at(page)->size(); ++row) {
            // column
            for (size_t col = 0; col < pages.at(page)->at(row).size(); ++col) {
                if (pages.at(page) == nullptr) {
                    continue;
                }

                config::ButtonBase* button = pages.at(page)->at(row).at(col);

                if (button == nullptr) {
                    continue;
                }

                this->sendMessage(launchpad::commands::led_on(commands::calculate_grid(row, col), button->get_color()));
            }

        }
    }
}