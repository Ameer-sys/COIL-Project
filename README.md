# 🚀 COIL Project -- Remote Robot Command & Control System

A distributed, cloud-integrated system for remotely commanding and
monitoring a robot across multiple networked environments using **C++,
REST APIs, and socket-based communication**.

This project was developed as part of the **Collaborative Online
International Learning (COIL)** initiative between\
**Conestoga College (Canada)** 🇨🇦 and **Fontys University
(Netherlands)** 🇳🇱.

------------------------------------------------------------------------

## 🌍 Overview

This system enables a user to control a remote robot through a **web
interface**, with communication routed through multiple machines using
configurable networking paths.

The project simulates real-world distributed systems involving: -
multi-hop communication - network routing logic - client-server
architecture - real-time command execution

------------------------------------------------------------------------

## 🧠 Key Features

-   🌐 Web-Based Control Interface (REST API driven)
-   🔁 Multi-Configuration Routing System (Config 1--3)
-   📡 TCP/UDP Communication with Robot Simulator
-   🔗 Relay-Based Architecture (Multi-PC Communication)
-   📦 Custom Packet Structure (PktDef)
-   🧪 Comprehensive Unit Testing (60+ tests, 100% pass rate)
-   🐳 Dockerized Deployment
-   📝 Logging & Debugging System

------------------------------------------------------------------------

## 🏗️ System Architecture - Configuration 3

Browser (GUI) 
↓ 
Web Server (PC2 - Crow REST API)
↓ 
\[Routing Logic Layer\] 
↓ 
PC3 (Relay / Next-Hop Server)
↓ 
Robot 1/2

------------------------------------------------------------------------

## ⚙️ How It Works

1.  User interacts with the web GUI
2.  A REST request is sent (e.g., /telecommand/)
3.  Server checks configuration:
    -   If routing OFF → send directly to robot
    -   If routing ON → forward request to next-hop (PC3)
4.  Packet is built using PktDef
5.  Communication handled via MySocket (TCP/UDP)
6.  Robot responds with ACK/data
7.  Response flows back → browser

------------------------------------------------------------------------

## 🧩 Core Components

### PktDef

Defines the structure of communication packets.

### MySocket

Custom socket abstraction supporting TCP & UDP.

### Web Server (Crow)

Handles REST endpoints, routing, and responses.

### GUI

User interface for sending commands and viewing telemetry.

------------------------------------------------------------------------

## 🧪 Testing

-   60+ Unit Tests (MSTest)
-   All tests passing
-   High coverage with edge-case handling

------------------------------------------------------------------------

## 🐳 Deployment

docker build -t coil-project . 
docker run -p 3540:3540 coil-project

------------------------------------------------------------------------

## 🧑‍💻 Technologies Used

-   C++
-   Crow (C++ Web Framework)
-   TCP/UDP Sockets
-   Docker
-   MSTest
-   REST APIs
-   JSON

------------------------------------------------------------------------

## 🤝 Collaboration

Developed as part of COIL: - Conestoga College - Fontys University

------------------------------------------------------------------------

## 💡 Highlights

-   Scalable relay architecture
-   Real-time communication system
-   Full-stack system (GUI → backend → network → simulator)

------------------------------------------------------------------------

## 📌 Future Improvements

-   Add authentication & security
-   Deploy to cloud
-   Improve telemetry UI

------------------------------------------------------------------------

## 🙌 Acknowledgements

Thanks to professors, teammates, and the COIL program.

------------------------------------------------------------------------

## ⭐ Final Note

This project demonstrates system design, networking, and global
collaboration.
