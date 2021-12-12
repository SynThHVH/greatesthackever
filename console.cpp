#include "includes.h"

// hopefully this will work as i have no clue how else to do it :)
void console::initialize(const char* title) {
	AllocConsole();

	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(0)), XOR("conin$"), XOR("r"), static_cast<_iobuf*>(__acrt_iob_func(0)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(1)), XOR("conout$"), XOR("w"), static_cast<_iobuf*>(__acrt_iob_func(1)));
	freopen_s(reinterpret_cast<_iobuf**>(__acrt_iob_func(2)), XOR("conout$"), XOR("w"), static_cast<_iobuf*>(__acrt_iob_func(2)));

	SetConsoleTitleA(title);

	// mr simon; do do du do du; bring me a dream; bum bum bum bum; make me the cutest, that i've ever ever seen; bum bum bum bum
	g_console.log(XOR("welcome tester!\nkeep an eye on this console while playing as this will help me figure out whats going wrong with the cheat!\nthis console will get updated and have more features at some point (dumping the _asm registers on crash, making this have commands you can execute, and suspending the process and dumping the console before closing)\nas the console is updated i will leave the update logs here and say what was changed, so keep an eye on this as this is a vital thing for you staging users to have!"));

	//std::array<std::string, 3> usercmds{ XOR("cmd"), XOR("test"), XOR("test2") };
	//g_console.log(XOR("for a list of commands, type cmd\n"));

	//std::string userinput;
	//std::cin >> userinput;

	//if (GetAsyncKeyState(VK_RETURN) && &userinput != usercmds.data()) {
	//	g_console.log("type a valid command nigger");
	//}

	//// table of contents
	//if (userinput == XOR("cmd")) {
	//	for (int i = 0; i < usercmds.size(); i++) { std::cout << usercmds[i] << std::endl; }
	//}

	//// now run our first command
	//else if (userinput == XOR("test")) {
	//	g_console.log("test ran sucessfully!");
	//}
}

void console::release() {
	fclose(static_cast<_iobuf*>(__acrt_iob_func(0)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(1)));
	fclose(static_cast<_iobuf*>(__acrt_iob_func(2)));

	FreeConsole();
}