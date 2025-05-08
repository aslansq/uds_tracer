This tool is designed for listening to CAN traffic and identifying UDS (Unified Diagnostic Services) packages. It provides a graphical interface to help users analyze and validate the data efficiently. It supports CAN Standard (CAN Std), CAN Flexible Data-rate (CAN FD), and replaying recorded data for detailed analysis.  

### Supported Devices

This tool supports the following devices for CAN communication:
- PEAK-System USB CAN interfaces

![GUI Overview](doc/assets/gui.gif)

### User Manual
For detailed usage instructions and advanced features, refer to the [Wiki page](https://github.com/aslansq/uds_tracer/wiki) of this repository.

### Requirements

To use this tool, ensure the following dependencies are installed:

- **Qt Framework**: Required for the graphical user interface. Install the Qt5 version for your platform.
- **PEAK-System Drivers**: Necessary for communication with PEAK-System USB CAN interfaces. Download and install the drivers from the [PEAK-System website](https://www.peak-system.com/).
- **Python 3**: Required for generating UDS (Unified Diagnostic Services) messages. Ensure Python 3 is installed and accessible in your system's PATH.

### Compilation

To compile the tool, navigate to the main directory and run the `build.sh` script:

```bash
./build.sh
```

### Source Code Documentation

1. To properly view the system's architecture and documentation, use Doxygen to generate the documentation. Follow these steps:
```bash
cd doc
doxygen doxyfile
```
2. Open the generated `index.html` file located in the `doc/output/html` directory in your web browser to explore the documentation.


