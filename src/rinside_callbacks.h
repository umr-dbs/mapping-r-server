/*
 * This file is part of mapping-r-server (https://github.com/umr-dbs/mapping-r-server).
 * Copyright (c) 2018 Database Research Group of the University of Marburg.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

class RInsideCallbacks : public Callbacks {
	public:
		// see inst/includes/Callbacks.h for a list of all overrideable methods
		virtual std::string ReadConsole( const char* prompt, bool addtohistory ) {
			return "";
		};

		virtual void WriteConsole( const std::string& line, int type ) {
			output_buffer << line;
			std::string trimmed = line;
			trimmed.erase(trimmed.find_last_not_of(" \n\r\t")+1);
			//Log::debug("R output, type %d: '%s'", type, trimmed.c_str());
		};

		virtual void FlushConsole() {
		};

		virtual void ResetConsole() {
		};

		virtual void CleanerrConsole() {
		};

		virtual void Busy( bool /*is_busy*/ ) {
		};

		virtual void ShowMessage(const char* message) {
			Log::info("R Message: '%s'", message);
		};

		virtual void Suicide(const char* message) {
			throw OperatorException(message); // TODO: is this the correct way to handle suicides?
		};


		virtual bool has_ReadConsole() { return true; };
		virtual bool has_WriteConsole() { return true; };
		virtual bool has_FlushConsole(){ return true; };
		virtual bool has_ResetConsole() { return true; };
		virtual bool has_CleanerrConsole() { return true; };
		virtual bool has_Busy() { return true; };
		virtual bool has_ShowMessage() { return true; };
		virtual bool has_Suicide() { return true; };

		void resetConsoleOutput() {
			output_buffer.str("");
			output_buffer.clear();
		}

		std::string getConsoleOutput() {
			return output_buffer.str();
		}
	private:
		std::ostringstream output_buffer{};
};
