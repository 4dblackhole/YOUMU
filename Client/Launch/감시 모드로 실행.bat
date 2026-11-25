@echo off
rem Starts the game in watch mode for this run only: device removed data on and one line of statistics
rem per second in Data\Logs. Starting the EXE itself is the normal mode. Option.ini is not changed.
start "" "%~dp0Y's Orchestral Unison - Melodic Ultimatum.exe" --diagnostics=watch
