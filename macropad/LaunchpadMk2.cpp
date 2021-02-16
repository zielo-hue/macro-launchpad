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
    _DebugString("There are " + std::to_string(nPorts) + " MIDI input sources available.\n");
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

    this->setup_pages_test();
    this->fullLedUpdate();

    devices.push_back(this);
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

midi_device::launchpadmk2::message_type midi_device::launchpadmk2::input::message_type() {
    launchpadmk2::message_type type =
        static_cast<launchpadmk2::message_type>(message.at(0) + message.at(2));

    // this shouldn't happen...
    if (type > message_type::automap_live_pressed || type < message_type::grid_depressed) {
        return message_type::invalid;
    }

    if (message.at(1) % 0x0A == 0x09) {
        if (type == message_type::grid_depressed) {
            return message_type::grid_page_change_depressed;
        }
        else if (type == message_type::grid_pressed) {
            return message_type::grid_page_change_pressed;
        }
    }

    return static_cast<launchpadmk2::message_type>(message.at(0) + message.at(2));
}

unsigned char midi_device::launchpadmk2::input::keycode() {
    return message.at(1);
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

        if (nBytes != 3)
            continue;

        input input = launchpadmk2::input(message);

		switch (input.message_type())
		{
        case message_type::grid_pressed:
	        {
	            this->sendMessageSysex(commands::led_setPalette(input.keycode(), 49), commands::led_setPalette_size);
	            break;
	        }
        case message_type::grid_depressed:
	        {
	            button = get_button(input.keycode());

        		if (button == nullptr)
        		{
	                this->sendMessageSysex(commands::led_off(input.keycode()), commands::led_setPalette_size);
        		}
        		else
        		{
	                button->execute();
	                this->sendMessageSysex(commands::led_set(input.keycode(), button->get_color()), commands::led_set_size);
        		}
	            break;
	        }
        case message_type::grid_page_change_pressed:
	        {
				// change page.
	            page = message[1] / 0x0B - 1;

        		// update all buttons
                this->fullLedUpdate();
	        }
        case message_type::automap_live_depressed:
			{
				break;
			}
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
        out->sendMessage(message, sizeof(unsigned char) * 5);
    }
    catch (RtMidiError& error) {
        error.printMessage();
        _DebugString(error.getMessage());
    }

cleanup:
    delete[] message;
}

// sysex test
void midi_device::launchpadmk2::LaunchpadMk2::sendMessageSysex(unsigned char* message, size_t size)
{
    unsigned char header[]{ 0xF0, 0x00, 0x20, 0x29, 0x02, 0x18 };

	// custom message payload
    std::vector<unsigned char> messageOut;

    if (!out->isPortOpen())
        goto cleanup;
	
	// add header to start
	messageOut.insert(messageOut.begin(), std::begin(header), std::end(header));

    for(size_t i = 0; i < size; ++i) {
        messageOut.push_back(message[i]);
    }

	// add header to end
    messageOut.push_back(static_cast<unsigned char>(0xF7));
	
	try
	{
        out->sendMessage(messageOut.data(), messageOut.size());
	}
	catch (RtMidiError& error)
	{
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
    out->sendMessage(commands::reset, sizeof(unsigned char) * 5);

    // clear page indicators
    for (size_t row = 0; row < 8; ++row)
    {
    	//try
    	//{
     //       pages.at(page);
    	//}
    	//catch (const std::out_of_range& e)
    	//{
     //       this->sendMessageSysex(commands::led_setPalette(commands::calculate_grid(row, 8), 14), 3);
    	//}
        this->sendMessageSysex(commands::led_setPalette(commands::calculate_grid(row, 8), 14), 3);
    }

    // set our page indicator to color 12, yellow
	// REFER TO THE MK2 PROGRAMMER'S MANUAL!!!! i should probably do this programatically
    this->sendMessageSysex(commands::led_setPalette(
        0x0A * page + 0x0A + 0x09,
        12), commands::led_setPalette_size);
	

    // set our "mode" indicator
    this->sendMessageSysex(new unsigned char[3]{ 0xB0, static_cast<unsigned char>(mode), 12 }, 3);

    // clear grid
    // this->sendMessageSysex(commands::led_setAll(0x00));
	
    // update every LEDs.
    if (static_cast<size_t>(page) < pages.size()) {

        // row
        for (size_t row = 0; row < pages.at(page)->size(); ++row) {
            // column
            for (size_t col = 0; col < pages.at(page)->at(row).size(); ++col) {
                if (pages.at(page) == nullptr) {
                    continue;
                }

                config::ButtonBase* button = pages.at(page)->at(row).at(col);

                if (button == nullptr) {
                    this->sendMessageSysex(commands::led_off(commands::calculate_grid(row, col)), 3);
                    continue;
                }
                // _DebugString(commands::led_set(commands::calculate_grid(row, col));
                this->sendMessageSysex(
                    commands::led_set(commands::calculate_grid(row, col),
                        button->get_color()), commands::led_set_size
                );
            }

        }
    }
}

void midi_device::launchpadmk2::LaunchpadMk2::setup_pages_test()
{
    launchpad_grid* page = new launchpad_grid{ nullptr };

    config::ButtonBase* button = new config::ButtonSimpleKeycodeTest(0x41);
    button->set_color(0x3F3F00);
    page->at(0)[0] = button;

    button = new config::ButtonComplexMacro([]() { _DebugString("lol\n"); });
    button->set_color(0x3F3A00);
	page->at(7)[6] = button;


https://onlineunicodetools.com/convert-unicode-to-hex use UCS-2-BE
    wchar_t* ste = new wchar_t[] {
        0xd14c, // (korean) te
            0xc2a4, // s
            0xd2b8, // t
            0x0021, // !
            0x0 // null terminator
    };
    std::wstring test = std::wstring(ste);
    button = new config::ButtonStringMacro(test);
    button->set_color(0x3A3A00);
    page->at(7)[5] = button;

    // mute
    button = new config::ButtonSimpleKeycodeTest(VK_F13);
    button->set_color(0x3F3F00);
    page->at(7)[0] = button;

    // deafen
    button = new config::ButtonSimpleKeycodeTest(VK_F14);
    button->set_color(0x3A0000);
    page->at(7)[1] = button;

    button = new config::ButtonSimpleKeycodeTest('a');
    button->set_color(0x3F3F00);
    page->at(6)[4] = button;

    pages.push_back(page);
}

void midi_device::launchpadmk2::LaunchpadMk2::load_config_buttons_test() {
    try {
        /*if (::config::config_file.at("devices").contains("Launchpad_S")) {
            return;
        }*/

        // why
        if (!::config::config_file.at("devices").at("Launchpad_MK2").is_object()) {
            return;
        }

        nlohmann::json& config = ::config::config_file.at("devices").at("Launchpad_MK2");
        pages.clear();
        // FIXME: hard limit of 8 pages by buttons but this should be handled better.
        pages.resize(8);

        for (auto& [page, buttons] : config.at("session").items()) {
            int index = std::stoi(page);
            launchpad_grid* page_buttons = new launchpad_grid{ nullptr };

            if (!buttons.is_array()) {
                _DebugString("lol you're fucked\n");
            }

            for (auto& button : buttons) {
                std::string type = button.at("type");
                int position_x = button.at("position").at(0);
                int position_y = button.at("position").at(1);
                unsigned int color = 0x221100;

                config::ButtonBase* new_button;

                if (type == "key_test") {
                    if (!button.at("data").is_number()) {
                        continue;
                    }

                    new_button = new config::ButtonSimpleKeycodeTest(button.at("data"));

                }
                else if (type == "key_string") {
                    if (!button.at("data").is_string()) {
                        continue;
                    }

                    new_button = new config::ButtonStringMacro(string_to_wstring(button.at("data")));
                }
                else {
                    new_button = new config::ButtonSimpleKeycodeTest('b');
                }

                new_button->set_color(color);

                page_buttons->at(position_x).at(position_y) = new_button;
            }

            pages.at(index) = page_buttons;
        }
    }
    catch (std::invalid_argument& e) {
        _DebugString("invalid args!\n");
    }
    catch (nlohmann::json::type_error& e) {
        _DebugString("type error!\n");
    }
    catch (nlohmann::json::out_of_range& e) {
        _DebugString("range error!\n");
    }

}

void midi_device::launchpadmk2::LaunchpadMk2::TerminateDevice()
{
    execute_all = false;
}

void midi_device::launchpadmk2::config::ButtonSimpleKeycodeTest::execute()
{
    if (keycode == -1) {
        return;
    }

    INPUT input;

    input.type = INPUT_KEYBOARD;
    input.ki.time = 0;
    input.ki.wScan = NULL;
    input.ki.dwExtraInfo = NULL;

    input.ki.wVk = keycode;
    input.ki.dwFlags = 0;

    // send
    SendInput(1, &input, sizeof(INPUT));

    // wait
    Sleep(100);

    // release
    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));

}

void midi_device::launchpadmk2::config::ButtonComplexMacro::execute()
{
    this->func();
}

void midi_device::launchpadmk2::config::ButtonStringMacro::execute()
{
    for (const wchar_t a : string) {
        INPUT input;

        input.type = INPUT_KEYBOARD;
        input.ki.time = 0;
        input.ki.wScan = a;
        input.ki.dwExtraInfo = NULL;

        input.ki.wVk = NULL;
        input.ki.dwFlags = KEYEVENTF_UNICODE;

        // send
        SendInput(1, &input, sizeof(INPUT));

        // wait
        Sleep(1);

        // release
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));

        Sleep(1);
    }
}

std::wstring midi_device::launchpadmk2::config::ButtonStringMacro::to_wstring()
{
    return L"midi_device::launchpadmk2::config::ButtonStringMacro : color=" + std::to_wstring(this->get_color()) + L" str=\"" + this->string + L"\"";
}


std::wstring midi_device::launchpadmk2::config::ButtonBase::to_wstring()
{
    return L"midi_device::launchpadmk2::config::ButtonBase : empty button";
}

std::wstring midi_device::launchpadmk2::config::ButtonComplexMacro::to_wstring()
{
    std::wstringstream buffer;
    buffer << std::hex << &this->func;

    return L"midi_device::launchpadmk2::config::ButtonComplexMacro : color=" + std::to_wstring(this->get_color()) + L" func_ptr=" + buffer.str();
}

std::wstring midi_device::launchpadmk2::config::ButtonSimpleKeycodeTest::to_wstring()
{
    return L"midi_device::launchpadmk2::config::ButtonSimpleKeycodeTest : color=" + std::to_wstring(this->get_color()) + L" keycode=" + std::to_wstring(this->keycode);
}