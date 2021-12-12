#pragma once

class console {
public: // backend functions
	void initialize(const char* title);
	void release();

public: // user functions
	// DO NOT CALL IN A FUCKING LOOP
	template <typename ... Args>
	void log(char const* const format, Args const& ... args) {
		printf(format, args ...);

		std::ofstream printed_file;
		if (!printed_file.is_open()) {
			printed_file.open("C:\\sonthdebug.txt", std::ios::out);
		}

		static bool m_finished_printing = false, m_error_occured = false;
		// do not do any fucking while statements in this please; you'll see why if you do.
		// just one more alright?
		if (printed_file.is_open()) {
			if (printed_file.good()) {
				printed_file << format << std::endl;
				m_finished_printing = true;
			}

			// this might not return bad since we call it if the file is open? idk
			else if (printed_file.bad()) {
				printed_file << "std::bad; contact an admin" << std::endl;
				m_error_occured = true;
			}

			// eof error; they just need to delete the file.
			else if (printed_file.eof()) {
				printed_file << "std::eof; send this file to an admin and delete it" << std::endl;
				m_error_occured = true;
			}

			// failed to generate output; return.
			else if (printed_file.fail()) {
				printed_file << "invalid output; skipping the rest of the line." << std::endl;
				m_error_occured = true;
			}

			// what the fuck happened here?
			//else m_error_occured = true;

			if (m_finished_printing || m_error_occured) printed_file.close();
		}

		// this will print it; but it will only print one line, so it's not what i want. try again
		//if (printed_file.is_open()) {
		//	printed_file << format << std::endl;
		//	printed_file.close(); // this will save our document and close it after it is done logging it.
		//}
	}
};
extern console g_console;