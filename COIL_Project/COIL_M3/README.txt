CSCN72050 - COIL Project Milestone 3
Group: Group-15 - CRC Syndicate
Members: Ishaq Ishaq Nasiru
         Shumroz Usmani
         Vineet Yogeshkumar Vaidya 
         Aryan Pramod Passi 

OVERVIEW

This project implements a Command-and-Control (C&C) system that allows a web-based GUI to communicate with a remote robot simulator.

The system uses:
* A C++ backend server built with Crow (REST API)
* Docker for deployment
* UDP/TCP communication with the simulator
* A browser-based GUI for user interaction
The application supports multiple configurations and provides real-time command execution, response handling, and logging.

--- REQUIREMENTS

* Docker Desktop installed
* Web browser (Chrome recommended)
* Conestoga VPN (required if off-campus)

--- HOW TO RUN THE PROJECT

1. Open terminal in the project folder

2. Build Docker image:
   docker build -t coil-server .

3. Run the container:
   docker run -p 3540:3540 coil-server

4. Open browser:
   http://localhost:3540

--- SIMULATOR CONNECTION

Simulator IP: 10.172.41.150
UDP Port: 29500
TCP Port: 29000

If off-campus:

* Connect to Conestoga VPN before running the system

--- USAGE
1. Select Configuration Mode (1, 2, or 3)
2. Select Target (Simulator)
3. Enter IP and Port
4. Click "Apply Connection"
5. Use control panel to send commands:

   * Drive Forward
   * Drive Backward
   * Turn Left
   * Turn Right
   * Sleep (Reset)
   * Request Telemetry

6. Observe system output:
   * Last Command Response
   * Robot Status (ACK, CRC, Packet, Body)
   * Packet Log (incoming/outgoing communication)

--- FEATURES
* Real-time command and response handling
* Support for UDP and TCP communication
* Dynamic configuration switching without restart
* Structured logging of all communication:

  * REST requests
  * Commands sent to robot
  * Responses received from robot

* Log export to text file
* Clean and user-friendly GUI interface

--- SYSTEM ARCHITECTURE
Client (Browser GUI) -> HTTP (REST) -> Server (Docker - C++ Crow Web Service) -> UDP / TCP -> Robot Simulator (Remote System)

--- NOTES
* The system communicates directly with the cloud-based simulator
* All communication and responses are handled and displayed within the GUI
* Ensure VPN is active if accessing simulator from outside campus network

