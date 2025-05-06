\page Cli

## Command Line Interface (CLI) Usage

The `uds_tracer` application provides a command-line interface with the following options:

```
Usage: ./uds_tracer [options]
UdsTracerCli

Options:
	-h, --help                 Displays help on commandline options.
	--help-all                 Displays help including Qt specific options.
	-v, --version              Displays version information.
	-l, --loadCommands <file>  Load commands from a specified file.
	-c, --cli                  Cli Mode
	-s, --show                 Show commands
```

### Example Command File

```
[
	{"loadConfig":"default.xml"},
	{"baud":"500000", "devStd":"/dev/pcanusb32"},
	{"devReplay":"20250505_190901.cobs"},
	{"canType":"Replay"}
]
```
