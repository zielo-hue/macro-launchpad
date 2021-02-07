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
		class ConfigFile
		{
			
		};

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
		void fullLedUpdate();
		void setup_pages();

		void reset();

		launchpad_grid* getCurrentButtons()
		{
			if (page >= pages.size())
				return nullptr;

			return pages.at(page);
		}

		static void RunDevice();
		static void TerminateDevice();

		static LaunchpadMk2* GetDevice() { return main_device; }
	};

	namespace commands
	{
		constexpr unsigned char vel_off = 0x00;

		// we're going bottom to top unlike top to bottom..
		// REMEMBER bottom left starts with 0x0B!! not 0x00
		inline unsigned char calculate_grid(unsigned char row, unsigned char column)
		{
			return (0x0A * row) + column;
		}

		inline void calculate_xy_from_keycode(unsigned char keycode, int &x, int &y)
		{
			x = keycode / 0x0A;
			y = keycode % 0x0A;
		}
	}
}