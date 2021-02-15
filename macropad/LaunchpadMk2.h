#pragma once
#include "RtMidi.h"
#include "MidiDevice.h"
#include <wchar.h>
#include <functional>

// the launching of pad mark ii
namespace midi_device::launchpadmk2
{
    enum class mode {
        session = 0x00,
        user1,
        user2,
        mixer
    };

	namespace config
	{
		class ButtonBase
		{
			unsigned int color;

		public:
			ButtonBase() : color(0x00FF00) {}
			virtual ~ButtonBase() {}
			virtual void execute() = 0;
			virtual std::wstring to_wstring() = 0;
			void set_color(unsigned int col) { color = col; }
			unsigned int get_color() { return color; }
		};

		class ButtonSimpleKeycodeTest : public ButtonBase {
			int keycode;
		public:
			ButtonSimpleKeycodeTest() : keycode(-1) {}
			ButtonSimpleKeycodeTest(int keycode) : keycode(keycode) {}
			void execute();
			std::wstring to_wstring();
		};

		typedef std::function<void()> ComplexMacroFn;

		class ButtonComplexMacro : public ButtonBase {
			ComplexMacroFn func;
		public:
			ButtonComplexMacro(ComplexMacroFn fun) : func(fun) {}
			void execute();
			std::wstring to_wstring();
		};

		class ButtonStringMacro : public ButtonBase {
			std::wstring string;
		public:
			ButtonStringMacro(std::wstring str) : string(str) {}
			void execute();
			std::wstring to_wstring();
		};
	}

	typedef std::array<config::ButtonBase*, 8> launchpad_row;
	typedef std::array<std::array<config::ButtonBase*, 8>, 8> launchpad_grid;

	// parity
	extern bool execute_all;

	class LaunchpadMk2 : public MidiDeviceBase
	{
		inline static LaunchpadMk2* main_device;

		bool should_loop;
		void Loop();
		
		config::ButtonBase* get_button(unsigned char num);

		mode mode = mode::session;
		unsigned int page = 0;

		std::vector<launchpad_grid*> pages;

	public:
		LaunchpadMk2() : should_loop(true)
		{
			in = new RtMidiIn();
			out = new RtMidiOut();
		}

		void Init();
		void sendMessage(unsigned char* message);
		void sendMessageSysex(unsigned char* message);
		void fullLedUpdate();
		void setup_pages_test();

		void reset();

		void load_config_buttons_test();

		launchpad_grid* getCurrentButtons()
		{
			if (page >= pages.size())
				return nullptr;

			return pages.at(page);
		}

		static void RunDevice();
		static void TerminateDevice();

		static LaunchpadMk2* GetDevice()
		{
			return reinterpret_cast<LaunchpadMk2*>(midi_device::devices.at(0));
		}
	};

	enum class message_type
	{
		invalid = 0x0,
		grid_depressed = 0x90,
		grid_pressed = 0x90 + 0x7F,
		grid_page_change_depressed = 0x90 + 0x7F + 0x1,
		grid_page_change_pressed = 0x90 + 0x7F + 0x2,
		automap_live_depressed = 0xB0,
		automap_live_pressed = 0xB0 + 0x7F,
	};

	class input
	{
	public:
		std::vector<unsigned char> message;

		input(std::vector<unsigned char> msg) : message(msg) {};
		message_type message_type();
		unsigned char keycode();
	};

	namespace commands
	{
		// pre-calculated values.
		constexpr unsigned char vel_off = 0x00;

		// we're going bottom to top unlike top to bottom..
		// REMEMBER bottom left starts with 0x0B!! not 0x00
		inline unsigned char calculate_grid(unsigned char row, unsigned char column)
		{
			return (0x0A * row) + column;
		}

		inline void calculate_xy_from_keycode(unsigned char keycode, int &x, int &y)
		{
			x = keycode / 0x0A - 1;
			y = keycode % 0x0A - 1;
		}

		// use color palette
		inline unsigned char* led_setPalette(unsigned char key, unsigned char color)
		{
			return new unsigned char[3]{ 0x0A, key, color};
		}

		// RGB Values
		inline unsigned char* led_set(unsigned char key, unsigned int color) {
			return new unsigned char[5]
			{
				0x0B, key,
				static_cast<unsigned char>((color & 0xFF0000) >> 4),
				static_cast<unsigned char>((color & 0x00FF00) >> 2),
				static_cast<unsigned char>(color & 0x0000FF),
			};
		}

		inline unsigned char* led_off(unsigned char key)
		{
			return led_setPalette(key, 0);
		}

		// Use palette color values
		inline unsigned char* led_setAll(unsigned char color)
		{
			return new unsigned char[5]{ 0x0E, color };
		}

		constexpr unsigned char reset[5] = { 0xB0, 0x00, 0x00, 0x00, 0x00 };
	}
}