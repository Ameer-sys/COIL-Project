// main.cpp
// CSCN72050 - COIL Project Milestone 3
// CROW RESTful webserver - Command and Control GUI for robotic devices

#include "crow_all.h"
#include "PktDef/PktDef.h"
#include "MySocket/MySocket.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <algorithm>

// json string helper
std::string JsonGetString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(":", pos);
    if (pos == std::string::npos) return "";
    pos = json.find("\"", pos);
    if (pos == std::string::npos) return "";
    size_t start = pos + 1;
    size_t end = json.find("\"", start);
    if (end == std::string::npos) return "";
    return json.substr(start, end - start);
}

// json int helper
int JsonGetInt(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return 0;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return 0;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    return std::stoi(json.substr(pos));
}

// json bool helper
bool JsonGetBool(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(":", pos);
    if (pos == std::string::npos) return false;
    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n')) pos++;
    return json.substr(pos, 4) == "true";
}

// json object helper
std::string JsonGetObject(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "{}";
    pos = json.find("{", pos);
    if (pos == std::string::npos) return "{}";
    size_t depth = 1, i = pos + 1;
    while (i < json.size() && depth > 0) {
        if (json[i] == '{') depth++;
        if (json[i] == '}') depth--;
        i++;
    }
    return json.substr(pos, i - pos);
}

// escape json text
std::string EscapeJson(const std::string& s)
{
    std::ostringstream out;
    for (char c : s) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"':  out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) >= 32) out << c;
            break;
        }
    }
    return out.str();
}

// target config
struct TargetConfig {
    std::string ip;
    int port = 0;
    std::string protocol;
    std::string label;
};

// app config
struct AppConfig {
    int serverPort = 3540;
    int configuration = 1;
    std::string activeTarget = "simulator";
    TargetConfig robot1, robot2, simulator;
    bool routingEnabled = false;
    std::string nextHopIP;
    int nextHopPort = 3540;
};

AppConfig g_config;
std::mutex g_configMutex;

// packet counter
int g_packetCounter = 0;
std::mutex g_packetMutex;

int NextPacketCount()
{
    std::lock_guard<std::mutex> lock(g_packetMutex);
    return ++g_packetCounter;
}

// log entry
struct LogEntry {
    std::string timestamp;
    std::string direction;
    std::string type;
    std::string details;
    bool success = false;
};

std::vector<LogEntry> g_log;
std::mutex g_logMutex;
const int MAX_LOG = 200;

// last command state
std::string g_lastCommand = "None";
std::string g_lastCommandTime = "—";
std::string g_lastCommandTarget = "Simulator";
std::string g_lastCommandRoute = "Direct";
bool g_lastAck = false;
bool g_lastCRC = false;
int g_lastPktCount = 0;
std::string g_lastResult = "No command sent yet";
std::mutex g_lastMutex;

// telemetry cache
struct TelemetryCache {
    bool decoded = false;
    unsigned short lastPktCounter = 0;
    unsigned short currentGrade = 0;
    unsigned short hitCount = 0;
    unsigned short heading = 0;
    std::string lastCmd = "--";
    int lastCmdValue = 0;
    int lastCmdPower = 0;
    std::string message = "No telemetry received yet";
};

TelemetryCache g_telemetry;
std::mutex g_telemetryMutex;

// time helper
std::string NowStr()
{
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tmv, "%H:%M:%S");
    return ss.str();
}

// add log
void AddLog(const std::string& dir, const std::string& type,
            const std::string& details, bool success)
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_log.push_back({NowStr(), dir, type, details, success});
    if ((int)g_log.size() > MAX_LOG) {
        g_log.erase(g_log.begin());
    }
}

// clear log
void ClearServerLog()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    g_log.clear();
}

// save last command
void SetLastCommandResult(const std::string& command,
                          const std::string& target,
                          const std::string& route,
                          bool ack,
                          bool crc,
                          int pktCount,
                          const std::string& result)
{
    std::lock_guard<std::mutex> lock(g_lastMutex);
    g_lastCommand = command;
    g_lastCommandTime = NowStr();
    g_lastCommandTarget = target;
    g_lastCommandRoute = route;
    g_lastAck = ack;
    g_lastCRC = crc;
    g_lastPktCount = pktCount;
    g_lastResult = result;
}

// target label
std::string GetTargetDisplayName(const std::string& t)
{
    if (t == "robot1") return "Robot 1";
    if (t == "robot2") return "Robot 2";
    return "Simulator";
}

// little endian reader
unsigned short ReadU16LE(const unsigned char* p)
{
    return static_cast<unsigned short>(p[0] | (p[1] << 8));
}

// telemetry decode
bool DecodeTelemetryBody(const char* body, int len, TelemetryCache& out)
{
    if (body == nullptr || len < 11) return false;

    const unsigned char* b = reinterpret_cast<const unsigned char*>(body);

    unsigned short lastPkt = ReadU16LE(b + 0);
    unsigned short grade   = ReadU16LE(b + 2);
    unsigned short hits    = ReadU16LE(b + 4);
    unsigned short heading = ReadU16LE(b + 6);
    unsigned char lastCmd  = b[8];
    unsigned char lastVal  = b[9];
    unsigned char lastPwr  = b[10];

    std::string cmdName = "--";
    switch (lastCmd) {
    case 0: cmdName = "None"; break;
    case 1: cmdName = "Drive Forward"; break;
    case 2: cmdName = "Drive Backward"; break;
    case 3: cmdName = "Turn Right"; break;
    case 4: cmdName = "Turn Left"; break;
    default: cmdName = "Unknown"; break;
    }

    bool looksReasonable =
        (lastCmd <= 10);

    if (!looksReasonable) return false;

    out.decoded = true;
    out.lastPktCounter = lastPkt;
    out.currentGrade = grade;
    out.hitCount = hits;
    out.heading = heading;
    out.lastCmd = cmdName;
    out.lastCmdValue = static_cast<int>(lastVal);
    out.lastCmdPower = static_cast<int>(lastPwr);
    out.message = "Telemetry received successfully";
    return true;
}

// load config
void LoadConfig(const std::string& path = "config.json")
{
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "config.json not found, using defaults.\n";
        return;
    }

    std::string json((std::istreambuf_iterator<char>(f)),
                      std::istreambuf_iterator<char>());

    std::lock_guard<std::mutex> lock(g_configMutex);

    g_config.serverPort = JsonGetInt(json, "server_port");
    if (g_config.serverPort == 0) g_config.serverPort = 3540;

    g_config.configuration = JsonGetInt(json, "configuration");
    if (g_config.configuration == 0) g_config.configuration = 1;

    g_config.activeTarget = JsonGetString(json, "active_target");
    if (g_config.activeTarget.empty()) g_config.activeTarget = "simulator";

    auto parseTarget = [&](const std::string& key) -> TargetConfig {
        std::string obj = JsonGetObject(json, key);
        TargetConfig t;
        t.ip = JsonGetString(obj, "ip");
        t.port = JsonGetInt(obj, "port");
        t.protocol = JsonGetString(obj, "protocol");
        t.label = JsonGetString(obj, "label");
        return t;
    };

    g_config.robot1 = parseTarget("robot1");
    g_config.robot2 = parseTarget("robot2");
    g_config.simulator = parseTarget("simulator");

    std::string rt = JsonGetObject(json, "routing_table");
    g_config.routingEnabled = JsonGetBool(rt, "enabled");
    g_config.nextHopIP = JsonGetString(rt, "next_hop_ip");
    g_config.nextHopPort = JsonGetInt(rt, "next_hop_port");
    if (g_config.nextHopPort == 0) g_config.nextHopPort = 3540;
}

// save config
void SaveConfig(const std::string& path = "config.json")
{
    std::lock_guard<std::mutex> lock(g_configMutex);
    std::ofstream f(path);

    f << "{\n"
      << "    \"server_port\": " << g_config.serverPort << ",\n\n"
      << "    \"configuration\": " << g_config.configuration << ",\n\n"
      << "    \"robot1\": {\n"
      << "        \"ip\": \"" << g_config.robot1.ip << "\",\n"
      << "        \"port\": " << g_config.robot1.port << ",\n"
      << "        \"protocol\": \"" << g_config.robot1.protocol << "\",\n"
      << "        \"label\": \"" << g_config.robot1.label << "\"\n"
      << "    },\n\n"
      << "    \"robot2\": {\n"
      << "        \"ip\": \"" << g_config.robot2.ip << "\",\n"
      << "        \"port\": " << g_config.robot2.port << ",\n"
      << "        \"protocol\": \"" << g_config.robot2.protocol << "\",\n"
      << "        \"label\": \"" << g_config.robot2.label << "\"\n"
      << "    },\n\n"
      << "    \"simulator\": {\n"
      << "        \"ip\": \"" << g_config.simulator.ip << "\",\n"
      << "        \"port\": " << g_config.simulator.port << ",\n"
      << "        \"protocol\": \"" << g_config.simulator.protocol << "\",\n"
      << "        \"label\": \"" << g_config.simulator.label << "\"\n"
      << "    },\n\n"
      << "    \"active_target\": \"" << g_config.activeTarget << "\",\n\n"
      << "    \"routing_table\": {\n"
      << "        \"enabled\": " << (g_config.routingEnabled ? "true" : "false") << ",\n"
      << "        \"next_hop_ip\": \"" << g_config.nextHopIP << "\",\n"
      << "        \"next_hop_port\": " << g_config.nextHopPort << "\n"
      << "    }\n"
      << "}\n";
}

// active target
TargetConfig GetActiveTarget()
{
    std::lock_guard<std::mutex> lock(g_configMutex);
    if (g_config.activeTarget == "robot1") return g_config.robot1;
    if (g_config.activeTarget == "robot2") return g_config.robot2;
    return g_config.simulator;
}

// direct send
std::vector<char> SendToRobotDirect(PktDef& pkt)
{
    TargetConfig target = GetActiveTarget();
    ConnectionType ct = (target.protocol == "TCP") ? TCP : UDP;

    MySocket sock(CLIENT, target.ip, target.port, ct, 1024);

    if (ct == TCP) sock.ConnectTCP();

    char* raw = pkt.GenPacket();
    int len = pkt.GetLength();

    sock.SendData(raw, len);

    std::vector<char> resp(1024, 0);
    int rlen = sock.GetData(resp.data());
    resp.resize(rlen > 0 ? rlen : 0);

    if (ct == TCP) sock.DisconnectTCP();

    return resp;
}

// route forward
std::string ForwardRESTToNextHop(const std::string& route,
                                 const std::string& method,
                                 const std::string& body)
{
    std::string nextIP;
    int nextPort = 3540;
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        nextIP = g_config.nextHopIP;
        nextPort = g_config.nextHopPort;
    }

    if (nextIP.empty()) return "{\"error\":\"no next hop configured\"}";

    std::ostringstream req;
    req << method << " " << route << " HTTP/1.1\r\n"
        << "Host: " << nextIP << ":" << nextPort << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    std::string reqStr = req.str();

    MySocket sock(CLIENT, nextIP, nextPort, TCP, 4096);
    sock.ConnectTCP();
    sock.SendData(reqStr.c_str(), (int)reqStr.size());

    char recvBuf[4096] = {};
    int rlen = sock.GetData(recvBuf);
    sock.DisconnectTCP();

    if (rlen <= 0) return "{\"error\":\"no response from next hop\"}";

    std::string httpResp(recvBuf, rlen);
    size_t body_start = httpResp.find("\r\n\r\n");
    if (body_start == std::string::npos) return httpResp;
    return httpResp.substr(body_start + 4);
}

// build log json
std::string BuildLogJSON()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < g_log.size(); i++) {
        const auto& e = g_log[i];
        ss << "{"
           << "\"time\":\"" << EscapeJson(e.timestamp) << "\","
           << "\"dir\":\"" << EscapeJson(e.direction) << "\","
           << "\"type\":\"" << EscapeJson(e.type) << "\","
           << "\"details\":\"" << EscapeJson(e.details) << "\","
           << "\"success\":" << (e.success ? "true" : "false")
           << "}";
        if (i + 1 < g_log.size()) ss << ",";
    }
    ss << "]";
    return ss.str();
}

// build log text
std::string BuildLogText()
{
    std::lock_guard<std::mutex> lock(g_logMutex);
    std::ostringstream ss;
    ss << "COIL Command and Control Log\n";
    ss << "============================\n";
    for (const auto& e : g_log) {
        ss << "[" << e.timestamp << "] "
           << e.direction << " "
           << e.type << " "
           << e.details
           << " | success=" << (e.success ? "true" : "false")
           << "\n";
    }
    return ss.str();
}

// extract printable body text
std::string ExtractBodyText(PktDef& pkt)
{
    char* body = pkt.GetBodyData();
    if (!body) return "";

    int size = pkt.GetLength() - HEADERSIZE - 1;
    if (size <= 0) return "";

    std::string out;
    for (int i = 0; i < size; i++) {
        unsigned char c = (unsigned char)body[i];
        if (c >= 32 && c <= 126)
            out += (char)c;
    }
    return out;
}

int main()
{
    LoadConfig();
    crow::SimpleApp app;

    // serve gui
    CROW_ROUTE(app, "/")([]() {
        std::ifstream f("GUI/index.html");
        if (!f.is_open()) return crow::response(404, "GUI not found");

        std::string html((std::istreambuf_iterator<char>(f)),
                          std::istreambuf_iterator<char>());

        crow::response res(html);
        res.add_header("Content-Type", "text/html");
        return res;
    });

    // serve gui files
    CROW_ROUTE(app, "/GUI/<string>")([](const std::string& filename) {
        std::ifstream f("GUI/" + filename);
        if (!f.is_open()) return crow::response(404, "File not found");

        std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());

        crow::response res(content);
        if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".css") {
            res.add_header("Content-Type", "text/css");
        } else if (filename.size() >= 3 && filename.substr(filename.size() - 3) == ".js") {
            res.add_header("Content-Type", "application/javascript");
        }
        return res;
    });

    // connect
    CROW_ROUTE(app, "/connect")
        .methods(crow::HTTPMethod::POST)
        ([](const crow::request& req) {
            AddLog("REST_IN", "CONNECT", "POST /connect received", true);

            auto body = crow::json::load(req.body);
            if (!body) {
                AddLog("REST_OUT", "CONNECT", "400 invalid JSON returned to client", false);
                return crow::response(400, "{\"error\":\"invalid JSON\"}");
            }

            std::string ip = body.has("ip") ? std::string(body["ip"].s()) : "";
            int port = body.has("port") ? body["port"].i() : 0;
            std::string protocol = body.has("protocol") ? std::string(body["protocol"].s()) : "UDP";

            if (ip.empty() || port <= 0) {
                AddLog("REST_OUT", "CONNECT", "400 invalid target returned to client", false);
                return crow::response(400, "{\"error\":\"invalid target\"}");
            }

            std::string targetName;
            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                targetName = g_config.activeTarget;
                if (g_config.activeTarget == "robot1") {
                    g_config.robot1.ip = ip;
                    g_config.robot1.port = port;
                    g_config.robot1.protocol = protocol;
                } else if (g_config.activeTarget == "robot2") {
                    g_config.robot2.ip = ip;
                    g_config.robot2.port = port;
                    g_config.robot2.protocol = protocol;
                } else {
                    g_config.simulator.ip = ip;
                    g_config.simulator.port = port;
                    g_config.simulator.protocol = protocol;
                }
            }

            SaveConfig();
            AddLog("SYS", "CONNECT", "Target set to " + ip + ":" + std::to_string(port) + " via " + protocol, true);
            AddLog("REST_OUT", "CONNECT", "200 OK returned to client", true);

            std::ostringstream json;
            json << "{"
                 << "\"status\":\"ok\","
                 << "\"ip\":\"" << EscapeJson(ip) << "\","
                 << "\"port\":" << port << ","
                 << "\"protocol\":\"" << EscapeJson(protocol) << "\","
                 << "\"target\":\"" << EscapeJson(targetName) << "\""
                 << "}";

            crow::response res(json.str());
            res.add_header("Content-Type", "application/json");
            return res;
        });

    // legacy connect route
    CROW_ROUTE(app, "/connect/<string>/<int>")
        .methods(crow::HTTPMethod::POST)
        ([](const std::string& ip, int port) {
            crow::json::wvalue body;
            body["ip"] = ip;
            body["port"] = port;
            body["protocol"] = "UDP";
            crow::response res;
            return crow::response(307);
        });

    // telecommand
    CROW_ROUTE(app, "/telecommand/")
        .methods(crow::HTTPMethod::PUT)
        ([](const crow::request& req) {
            AddLog("REST_IN", "TELECOMMAND", "PUT /telecommand/ received", true);

            auto body = crow::json::load(req.body);
            if (!body) {
                AddLog("REST_OUT", "TELECOMMAND", "400 invalid JSON returned to client", false);
                return crow::response(400, "{\"error\":\"invalid JSON\"}");
            }

            std::string cmd = body["cmd"].s();

            PktDef pkt;
            pkt.SetPktCount(NextPacketCount());

            std::string logDetails;
            std::string commandLabel;
            std::string commandType;

            if (cmd == "drive") {
                std::string dir = body["direction"].s();
                int duration = body["duration"].i();
                int power = body["power"].i();

                pkt.SetCmd(DRIVE);

                if (dir == "forward" || dir == "backward") {
                    DriveBody db;
                    db.Direction = (dir == "forward") ? FORWARD : BACKWARD;
                    db.Duration = static_cast<unsigned char>(duration);
                    db.Power = static_cast<unsigned char>(power);
                    pkt.SetBodyData(reinterpret_cast<char*>(&db), sizeof(DriveBody));

                    logDetails = "Command: Drive " + std::string(dir == "forward" ? "Forward" : "Backward") +
                                 " | Duration: " + std::to_string(duration) +
                                 "s | Power: " + std::to_string(power) + "%";
                    commandLabel = "Drive " + dir;
                    commandType = (dir == "forward") ? "DRIVE_FORWARD" : "DRIVE_BACKWARD";
                } else {
                    TurnBody tb;
                    tb.Direction = (dir == "left") ? LEFT : RIGHT;
                    tb.Duration = static_cast<unsigned short>(duration);
                    pkt.SetBodyData(reinterpret_cast<char*>(&tb), sizeof(TurnBody));

                    logDetails = "Command: Turn " + std::string(dir == "left" ? "Left" : "Right") +
                                 " | Duration: " + std::to_string(duration) + "s";
                    commandLabel = "Turn " + dir;
                    commandType = (dir == "left") ? "TURN_LEFT" : "TURN_RIGHT";
                }
            } else if (cmd == "sleep") {
                pkt.SetCmd(SLEEP);
                logDetails = "Command: Sleep / Reset";
                commandLabel = "Sleep";
                commandType = "SLEEP";
            } else {
                AddLog("REST_OUT", "TELECOMMAND", "400 unknown command returned to client", false);
                return crow::response(400, "{\"error\":\"unknown command\"}");
            }

            AddLog("OUT", commandType, logDetails, true);

            bool routing = false;
            std::string targetName;
            std::string routeText = "Direct";

            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                routing = (g_config.configuration == 3 && g_config.routingEnabled && !g_config.nextHopIP.empty());
                targetName = GetTargetDisplayName(g_config.activeTarget);
                if (routing) {
                    routeText = "Routed via " + g_config.nextHopIP + ":" + std::to_string(g_config.nextHopPort);
                }
            }

            if (routing) {
                AddLog("OUT", "ROUTE", "Forwarding command to next hop", true);
                std::string result = ForwardRESTToNextHop("/telecommand/", "PUT", req.body);
                AddLog("IN", "ROUTE", "Response received from next hop", true);

                SetLastCommandResult(commandLabel, targetName, routeText, true, true, pkt.GetPktCount(),
                                     "Command forwarded through next hop successfully");

                AddLog("REST_OUT", "TELECOMMAND", "200 OK returned to client", true);
                crow::response res(result);
                res.add_header("Content-Type", "application/json");
                return res;
            }

            auto resp = SendToRobotDirect(pkt);

bool ack = false;
bool crc = false;
int respPktNum = 0;
std::string resultText = "No response received within timeout period";
std::string bodyText = "";

if (!resp.empty()) {
    PktDef r(resp.data());

    ack = r.GetAck();
    crc = r.CheckCRC(resp.data(), (int)resp.size());
    respPktNum = r.GetPktCount();

    bodyText = ExtractBodyText(r);

    if (!ack) {
        AddLog("IN", "NACK", "Command rejected", false);
        resultText = bodyText.empty() ? "Command rejected (NACK)" : bodyText;
    }
    else {
        AddLog("IN", "ACK",
            "PktCount=" + std::to_string(respPktNum) + " CRC=" + (crc ? "OK" : "FAIL"),
            ack && crc);

        resultText = bodyText.empty()
            ? "Command executed successfully"
            : bodyText;
    }
}
else {
    AddLog("IN", "TIMEOUT", "No response received", false);
}

// build JSON
std::ostringstream json;
json << "{"
     << "\"ack\":" << (ack ? "true" : "false") << ","
     << "\"crc\":" << (crc ? "true" : "false") << ","
     << "\"pktCount\":" << respPktNum << ","
     << "\"bodyText\":\"" << EscapeJson(bodyText) << "\","
     << "\"message\":\"" << EscapeJson(resultText) << "\""
     << "}";

crow::response res(json.str());
res.add_header("Content-Type", "application/json");
return res;
        });

    // telemetry
    CROW_ROUTE(app, "/telementry_request/")
        .methods(crow::HTTPMethod::GET)
        ([]() {
            AddLog("REST_IN", "TELEMETRY", "GET /telementry_request/ received", true);

            PktDef pkt;
            pkt.SetPktCount(NextPacketCount());
            pkt.SetCmd(RESPONSE);

            AddLog("OUT", "TELEMETRY", "Requesting housekeeping telemetry", true);

            bool routing = false;
            std::string targetName;
            std::string routeText = "Direct";

            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                routing = (g_config.configuration == 3 && g_config.routingEnabled && !g_config.nextHopIP.empty());
                targetName = GetTargetDisplayName(g_config.activeTarget);
                if (routing) {
                    routeText = "Routed via " + g_config.nextHopIP + ":" + std::to_string(g_config.nextHopPort);
                }
            }

            if (routing) {
                AddLog("OUT", "ROUTE", "Forwarding telemetry request to next hop", true);
                std::string result = ForwardRESTToNextHop("/telementry_request/", "GET", "");
                AddLog("IN", "ROUTE", "Telemetry response received from next hop", true);

                SetLastCommandResult("Telemetry Request", targetName, routeText, true, true, pkt.GetPktCount(),
                                     "Telemetry request forwarded through next hop successfully");

                AddLog("REST_OUT", "TELEMETRY", "200 OK returned to client", true);
                crow::response res(result);
                res.add_header("Content-Type", "application/json");
                return res;
            }



            auto resp = SendToRobotDirect(pkt);
            bool ack = false;
            bool crc = false;
            int respPktNum = 0;
            std::string resultText = "No response received within timeout period";
            std::string bodyText = "";

            // read the size of the header to determine if we got a response at all
            // shift resp + sizeof(PktHeader) to get to the body, if we have a response
            // if we got a respose display the info to ur website 

            if (resp.empty()) {
                AddLog("IN", "TIMEOUT", "No telemetry response (timeout - check VPN/network)", false);

                {
                    std::lock_guard<std::mutex> tlock(g_telemetryMutex);
                    g_telemetry.decoded = false;
                    g_telemetry.message = "No telemetry response received within timeout period";
                }

                SetLastCommandResult("Telemetry Request", targetName, routeText, false, false, 0,
                                     "No telemetry response received within timeout period");
                AddLog("REST_OUT", "TELEMETRY", "504 no response returned to client", false);
                return crow::response(504, "{\"error\":\"no response from robot\"}");
            }

            PktDef r(resp.data());
            crc = r.CheckCRC(resp.data(), (int)resp.size());
            int bodySize = r.GetLength() - HEADERSIZE - 1;
            char* bodyPtr = r.GetBodyData();
            bodyText = ExtractBodyText(r);

            AddLog("IN", "TELEMETRY", "Received " + std::to_string(resp.size()) + " bytes", crc);

            TelemetryCache temp;
            bool decoded = false;

unsigned short lastPkt = 0;
unsigned short grade = 0;
unsigned short hits = 0;
unsigned short heading = 0;
unsigned char lastCmd = 0;
unsigned char lastVal = 0;
unsigned char lastPwr = 0;

if (bodyPtr != nullptr && bodySize >= 11) {
    const unsigned char* b = reinterpret_cast<const unsigned char*>(bodyPtr);

    lastPkt = static_cast<unsigned short>(b[0] | (b[1] << 8));
    grade   = static_cast<unsigned short>(b[2] | (b[3] << 8));
    hits    = static_cast<unsigned short>(b[4] | (b[5] << 8));
    heading = static_cast<unsigned short>(b[6] | (b[7] << 8));
    lastCmd = b[8];
    lastVal = b[9];
    lastPwr = b[10];
}

            decoded = true;
            temp.decoded = true;
            temp.lastPktCounter = lastPkt;
            temp.currentGrade = grade;
            temp.hitCount = hits;
            temp.heading = heading;
            temp.lastCmd = std::to_string(lastCmd);
            temp.lastCmdValue = lastVal;
            temp.lastCmdPower = lastPwr;
            temp.message = crc ? "Telemetry received successfully" : "Telemetry received but CRC failed";

            if (!decoded && bodyPtr != nullptr) {
    std::string messageText = bodyText;

    if (messageText.empty()) {
        messageText = "STATUS Command Received - Transmitting Status Data Back";
    }

    temp.decoded = false;
    temp.message = messageText;
}

            {
                std::lock_guard<std::mutex> tlock(g_telemetryMutex);
                g_telemetry = temp;
            }

            resultText = temp.decoded
                ? "Telemetry received: Grade = " + std::to_string(temp.currentGrade)
                : temp.message;

            SetLastCommandResult("Telemetry Request", targetName, routeText, true, crc, r.GetPktCount(), resultText);

            std::ostringstream json;
            json << "{"
                 << "\"crc\":" << (crc ? "true" : "false") << ","
                 << "\"pktCount\":" << r.GetPktCount() << ","
                 << "\"telemetryDecoded\":" << (temp.decoded ? "true" : "false") << ","
                 << "\"lastPktCounter\":" << temp.lastPktCounter << ","
                 << "\"currentGrade\":" << temp.currentGrade << ","
                 << "\"hitCount\":" << temp.hitCount << ","
                 << "\"heading\":" << temp.heading << ","
                 << "\"lastCmd\":\"" << EscapeJson(temp.lastCmd) << "\","
                 << "\"lastCmdValue\":" << temp.lastCmdValue << ","
                 << "\"bodyText\":\"" << EscapeJson(bodyText) << "\","
                 << "\"lastCmdPower\":" << temp.lastCmdPower << ","
                 << "\"message\":\"" << EscapeJson(temp.message) << "\""
                 << "}";

            AddLog("REST_OUT", "TELEMETRY", "200 OK returned to client", crc);
            crow::response res(json.str());
            res.add_header("Content-Type", "application/json");
            return res;
        });

    // routing config
    CROW_ROUTE(app, "/routing_table/")
        .methods(crow::HTTPMethod::POST, crow::HTTPMethod::GET)
        ([](const crow::request& req) {
            if (req.method == crow::HTTPMethod::GET) {
                AddLog("REST_IN", "ROUTING", "GET /routing_table/ received", true);

                std::lock_guard<std::mutex> lock(g_configMutex);
                TargetConfig active;
                if (g_config.activeTarget == "robot1") active = g_config.robot1;
                else if (g_config.activeTarget == "robot2") active = g_config.robot2;
                else active = g_config.simulator;

                std::ostringstream json;
                json << "{"
                     << "\"enabled\":" << (g_config.routingEnabled ? "true" : "false") << ","
                     << "\"nextHopIP\":\"" << EscapeJson(g_config.nextHopIP) << "\","
                     << "\"nextHopPort\":" << g_config.nextHopPort << ","
                     << "\"configuration\":" << g_config.configuration << ","
                     << "\"protocol\":\"" << EscapeJson(active.protocol) << "\""
                     << "}";

                AddLog("REST_OUT", "ROUTING", "200 OK returned to client", true);
                crow::response res(json.str());
                res.add_header("Content-Type", "application/json");
                return res;
            }

            AddLog("REST_IN", "ROUTING", "POST /routing_table/ received", true);

            auto body = crow::json::load(req.body);
            if (!body) {
                AddLog("REST_OUT", "ROUTING", "400 invalid JSON returned to client", false);
                return crow::response(400, "{\"error\":\"invalid JSON\"}");
            }

            int newConfig = 1;
            bool newRouting = false;

            {
                std::lock_guard<std::mutex> lock(g_configMutex);
                if (body.has("configuration")) g_config.configuration = body["configuration"].i();
                if (body.has("enabled")) g_config.routingEnabled = body["enabled"].b();
                if (body.has("nextHopIP")) g_config.nextHopIP = std::string(body["nextHopIP"].s());
                if (body.has("nextHopPort")) g_config.nextHopPort = body["nextHopPort"].i();
                if (body.has("activeTarget")) g_config.activeTarget = std::string(body["activeTarget"].s());

                if (body.has("protocol")) {
                    std::string protocol = std::string(body["protocol"].s());
                    if (g_config.activeTarget == "robot1") g_config.robot1.protocol = protocol;
                    else if (g_config.activeTarget == "robot2") g_config.robot2.protocol = protocol;
                    else g_config.simulator.protocol = protocol;
                }

                newConfig = g_config.configuration;
                newRouting = g_config.routingEnabled;
            }

            SaveConfig();
            AddLog("SYS", "CONFIG",
                   "Configuration " + std::to_string(newConfig) +
                   " active, routing=" + (newRouting ? "on" : "off"), true);
            AddLog("REST_OUT", "ROUTING", "200 OK returned to client", true);

            crow::response res("{\"status\":\"ok\"}");
            res.add_header("Content-Type", "application/json");
            return res;
        });

    // clear log
    CROW_ROUTE(app, "/log/clear")
        .methods(crow::HTTPMethod::POST)
        ([]() {
            AddLog("REST_IN", "LOG", "POST /log/clear received", true);
            ClearServerLog();
            AddLog("SYS", "LOG", "Server log cleared", true);
            AddLog("REST_OUT", "LOG", "200 OK returned to client", true);

            crow::response res("{\"status\":\"ok\"}");
            res.add_header("Content-Type", "application/json");
            return res;
        });

    // download log
    CROW_ROUTE(app, "/log/download")
        .methods(crow::HTTPMethod::GET)
        ([]() {
            AddLog("REST_IN", "LOG", "GET /log/download received", true);

            crow::response res(BuildLogText());
            res.add_header("Content-Type", "text/plain");
            res.add_header("Content-Disposition", "attachment; filename=\"coil_log.txt\"");
            return res;
        });

    // status
    CROW_ROUTE(app, "/status")([]() {
        std::string target, ip, nextHopIP, protocol;
        int port = 0, config = 1, nextHopPort = 3540;
        bool routing = false;

        {
            std::lock_guard<std::mutex> lock(g_configMutex);
            target = g_config.activeTarget;
            config = g_config.configuration;
            routing = g_config.routingEnabled;
            nextHopIP = g_config.nextHopIP;
            nextHopPort = g_config.nextHopPort;

            if (target == "robot1") {
                ip = g_config.robot1.ip;
                port = g_config.robot1.port;
                protocol = g_config.robot1.protocol;
            } else if (target == "robot2") {
                ip = g_config.robot2.ip;
                port = g_config.robot2.port;
                protocol = g_config.robot2.protocol;
            } else {
                ip = g_config.simulator.ip;
                port = g_config.simulator.port;
                protocol = g_config.simulator.protocol;
            }
        }

        std::string lastCommand, lastTime, lastTarget, lastRoute, lastResult;
        bool lastAck, lastCRC;
        int lastPkt;
        {
            std::lock_guard<std::mutex> lock(g_lastMutex);
            lastCommand = g_lastCommand;
            lastTime = g_lastCommandTime;
            lastTarget = g_lastCommandTarget;
            lastRoute = g_lastCommandRoute;
            lastAck = g_lastAck;
            lastCRC = g_lastCRC;
            lastPkt = g_lastPktCount;
            lastResult = g_lastResult;
        }

        std::ostringstream json;
        json << "{"
             << "\"target\":\"" << EscapeJson(target) << "\","
             << "\"ip\":\"" << EscapeJson(ip) << "\","
             << "\"port\":" << port << ","
             << "\"protocol\":\"" << EscapeJson(protocol) << "\","
             << "\"configuration\":" << config << ","
             << "\"routing\":" << (routing ? "true" : "false") << ","
             << "\"nextHopIP\":\"" << EscapeJson(nextHopIP) << "\","
             << "\"nextHopPort\":" << nextHopPort << ","
             << "\"lastCommand\":\"" << EscapeJson(lastCommand) << "\","
             << "\"lastCommandTime\":\"" << EscapeJson(lastTime) << "\","
             << "\"lastCommandTarget\":\"" << EscapeJson(lastTarget) << "\","
             << "\"lastCommandRoute\":\"" << EscapeJson(lastRoute) << "\","
             << "\"lastAck\":" << (lastAck ? "true" : "false") << ","
             << "\"lastCRC\":" << (lastCRC ? "true" : "false") << ","
             << "\"lastPktCount\":" << lastPkt << ","
             << "\"lastResult\":\"" << EscapeJson(lastResult) << "\","
             << "\"log\":" << BuildLogJSON()
             << "}";

        crow::response res(json.str());
        res.add_header("Content-Type", "application/json");
        return res;
    });

    std::cout << "COIL Command and Control Server starting on port "
              << g_config.serverPort << std::endl;
    std::cout << "Configuration: " << g_config.configuration << std::endl;
    std::cout << "Active target: " << g_config.activeTarget << std::endl;

    app.bindaddr("0.0.0.0").port(g_config.serverPort).multithreaded().run();
    return 0;
}
