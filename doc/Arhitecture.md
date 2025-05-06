\page Arhitecture

## Architecture Overview

The architecture consists of three main components:

\ref Gui  
The **GUI** acts as a wrapper around the **CLI**. Providing a user-friendly interface while leveraging the functionality of the command-line interface.

\ref Cli  
**CLI**: Offers a command-line interface for advanced users and automation.  
All interactions facilitated through JSON objects.

\ref UdsGenerator  
**UDS Generator**: Handles the generation of UDS (Unified Diagnostic Services) message types.  
The **UDS Generator** is implemented as a Python script with support for Jinja2 templates.  
  
These templates allow for the generation of various outputs, including C code, to facilitate integration with different systems. This flexibility ensures that the tool can adapt to diverse project requirements.