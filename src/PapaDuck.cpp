/**
 * @file PapaDuck.cpp
 * @author Timo Wielink (ported to Linux by conversion)
 * @brief Uses built-in PapaDuck from the SDK to create a Linux Papa Duck
 *
 * Connects to AWS IoT over the OS-managed network interface (no WiFi management
 * needed). Forwards all mesh packets except ACK/RREQ/RREP to the cloud.
 * When the MQTT connection is lost, received packets are held in a queue and
 * flushed on reconnect. Queue depth is controlled by `QUEUE_SIZE_MAX`.
 *
 * @date 05-07-2025
 */

#include <string>
#include <queue>
#include <vector>
#include <array>
#include <cstdio>
#include <unistd.h>
#include <cstring>

#include "CDP.h"
#include "ArduinoReplacement.h"
#include "ArduinoTimerReplacement.h"

// MQTT over TLS — backed by Eclipse Paho C++ and WiFiClientSecure credential store.
#include "WiFiClientSecure.h"
#include "PubSubClient.h"
#include "secrets.h"

// --- Command Definitions ---
#define CMD_STATE_WIFI    "/wifi/"
#define CMD_STATE_HEALTH  "/health/"
#define CMD_STATE_CHANNEL "/channel/"

// --- Global Objects ---
PapaDuck duck(THINGNAME);
int QUEUE_SIZE_MAX = 5;
auto timer = timer_create_default();
bool retry = true;
const char commandTopic[] = "iot-2/cmd/+/fmt/+";
bool setupOK = false;

// --- Function Declarations ---
std::queue<std::vector<unsigned char>> packetQueue;
int quackJson(CdpPacket packet);
void handleDuckData(CdpPacket receivedPacket);
void dmsCmdReceived(char* topic, unsigned char* payload, unsigned int payloadLength);
void mqttConnect();
void subscribeTo(const char* topic);
bool enableRetry(void*);
void publishQueue();

// --- MQTT / TLS Client Setup ---
WiFiClientSecure wifiClient;
PubSubClient client(AWS_IOT_ENDPOINT, 8883, dmsCmdReceived, wifiClient);

/**
 * @brief Converts a received CDP packet into JSON format and publishes it to an MQTT topic.
 *
 * Parses key metadata fields from the incoming CdpPacket, encodes them into a structured
 * JSON document, serializes it, and publishes to:
 *   calpoly/device/THINGNAME/evt/<topic>
 *
 * @param packet A CDP packet received from the mesh network
 * @return int Returns 0 on successful publish; -1 on failure
 */
int quackJson(CdpPacket packet) {

    JsonDocument doc;

    std::string payload(packet.data.begin(), packet.data.end());
    std::string sduid(packet.sduid.begin(), packet.sduid.end());
    std::string dduid(packet.dduid.begin(), packet.dduid.end());
    std::string muid(packet.muid.begin(), packet.muid.end());

    cdpPrintf("[PAPA] Packet Received:\n");
    cdpPrintf("[PAPA] sduid:   %s\n", sduid.c_str());
    cdpPrintf("[PAPA] dduid:   %s\n", dduid.c_str());
    cdpPrintf("[PAPA] muid:    %s\n", muid.c_str());
    cdpPrintf("[PAPA] data:    %s\n", payload.c_str());
    cdpPrintf("[PAPA] hops:    %s\n", std::to_string(packet.hopCount).c_str());
    cdpPrintf("[PAPA] duck:    %s\n", std::to_string(packet.duckType).c_str());

    doc["DeviceID"]  = sduid;
    doc["MessageID"] = muid;
    doc["Payload"].set(payload);
    doc["hops"].set(packet.hopCount);
    doc["duckType"].set(packet.duckType);

    std::string cdpTopic = packet.topicToString();
    std::string topic = "calpoly/device/" + std::string(THINGNAME) + "/evt/" + cdpTopic;

    std::string jsonstat;
    serializeJson(doc, jsonstat);

    if (client.publish(topic.c_str(), jsonstat.c_str())) {
        cdpPrintf("[PAPA] Packet forwarded:\n");
        std::string prettyOut;
        serializeJsonPretty(doc, prettyOut);
        cdpPrintf("%s\n", prettyOut.c_str());
        cdpPrintf("[PAPA] Publish ok\n");
        return 0;
    } else {
        cdpPrintf("[PAPA] Publish failed\n");
        return -1;
    }
}

/**
 * @brief Callback invoked when a packet is received by the PapaDuck.
 *
 * Filters out ACK / RREQ / RREP packets, attempts to publish the rest via MQTT,
 * and queues any that fail for later retry.
 *
 * @param receivedPacket The CDP packet received from the mesh network
 */
void handleDuckData(CdpPacket receivedPacket) {
    cdpPrintf("[PAPA] got packet\n");

    if (receivedPacket.topic != reservedTopic::ack  &&
        receivedPacket.topic != reservedTopic::rrep &&
        receivedPacket.topic != reservedTopic::rreq) {

        if (quackJson(receivedPacket) == -1) {
            if ((int)packetQueue.size() > QUEUE_SIZE_MAX) {
                packetQueue.pop();
            }
            packetQueue.push(receivedPacket.data);
            cdpPrintf("[PAPA] New size of queue: %zu\n", packetQueue.size());
        }
    }

    subscribeTo(commandTopic);
}

/**
 * @brief Initializes the PapaDuck: radio, TLS credentials, and packet callback.
 *
 * Network connectivity is assumed to be managed by the OS. No WiFi join is
 * performed here.
 *
 * - Sets up the PapaDuck firmware with default radio settings.
 * - Registers handleDuckData as the incoming-packet callback.
 * - Loads TLS certificates into the credential store for MQTT over TLS.
 */
void setup() {
    duck.setupWithDefaults();

    duck.onReceiveDuckData(handleDuckData);

    cdpPrintf("[PAPA] Using root CA cert\n");
    wifiClient.setCACert(AWS_CERT_CA);
    wifiClient.setCertificate(AWS_CERT_CRT);
    wifiClient.setPrivateKey(AWS_CERT_PRIVATE);

    setupOK = true;
    cdpPrintf("[PAPA] Setup OK!\n");
}

/**
 * @brief Main loop: maintains MQTT connection, duck operation, and timers.
 *
 * Network connectivity is OS-managed; loop() only services MQTT keepalive,
 * the CDP radio layer, and the timer queue.
 */
void loop() {
    if (!setupOK) {
        return;
    }

    if (!client.loop()) {
        mqttConnect();
    }

    duck.run();
    timer.tick();
}

/**
 * @brief Entry point.
 */
int main(int argc, char* argv[]) {
    setup();
    cdpPrintf("[PAPA] Entering main loop...\n");

    while (1) {
        loop();
        usleep(100000); // 100 ms idle between iterations
    }

    return 0;
}

/**
 * @brief Handles incoming MQTT commands from the cloud (DMS).
 *
 * Parses the topic to determine command type (WiFi, Health, Channel)
 * and dispatches accordingly.
 *
 * @param topic         The MQTT topic string
 * @param payload       The raw command payload bytes
 * @param payloadLength Number of bytes in the payload
 */
void dmsCmdReceived(char* topic, unsigned char* payload, unsigned int payloadLength) {
    cdpPrintf("[PAPA] DMS Command Received for topic: %s\n", topic);

    if (std::string(topic).find(CMD_STATE_WIFI) != std::string::npos) {
        cdpPrintf("[PAPA] Start WiFi Command\n");
        unsigned char sCmd = 1;
        std::vector<unsigned char> sValue = {payload[0]};

        if (payloadLength > 3) {
            std::string destination(reinterpret_cast<char*>(payload), payloadLength);
            std::array<unsigned char, 8> dDevId;
            std::copy(destination.begin(), destination.end(), dDevId.begin());
            // duck.sendCommand(sCmd, sValue, dDevId);
        }
        // else { duck.sendCommand(sCmd, sValue); }

    } else if (std::string(topic).find(CMD_STATE_HEALTH) != std::string::npos) {
        unsigned char sCmd = 0;
        std::vector<unsigned char> sValue = {payload[0]};

        if (payloadLength >= 8) {
            // Destination starts at byte 1
            std::string destination(reinterpret_cast<char*>(payload + 1), payloadLength - 1);
            std::array<unsigned char, 8> dDevId;
            std::copy(destination.begin(), destination.end(), dDevId.begin());
            // duck.sendCommand(sCmd, sValue, dDevId);
        } else {
            cdpPrintf("[PAPA] Payload size too small\n");
        }

    } else {
        cdpPrintf("[PAPA] dmsCmdReceived: unexpected topic: %s\n", topic);
    }
}

/**
 * @brief Attempts to (re)connect the MQTT client to AWS IoT.
 *
 * If not connected, tries client.connect(). On failure schedules a retry
 * via the timer. On success, drains any queued packets and subscribes
 * to the command topic.
 */
void mqttConnect() {
    if (!client.connected()) {
        cdpPrintf("[PAPA] Reconnecting MQTT client to %s\n", AWS_IOT_ENDPOINT);
        if (!client.connect(THINGNAME) && retry) {
            cdpPrintf("[PAPA] Connection failed, retry in 5 seconds\n");
            retry = false;
            timer.in(5000, enableRetry);
        } else {
            if (!packetQueue.empty()) {
                publishQueue();
            }
            subscribeTo(commandTopic);
        }
    } else {
        if (!packetQueue.empty()) {
            publishQueue();
        }
        subscribeTo(commandTopic);
    }
}

/**
 * @brief Subscribes the MQTT client to the given topic and logs the result.
 *
 * @param topic The MQTT topic string to subscribe to
 */
void subscribeTo(const char* topic) {
    cdpPrintf("[PAPA] subscribe to %s", topic);
    if (client.subscribe(topic)) {
        cdpPrintf(" OK\n");
    } else {
        cdpPrintf(" FAILED\n");
    }
}

/**
 * @brief Re-enables the MQTT retry flag after a cooldown period.
 *
 * Used as a timer callback to throttle reconnect attempts.
 *
 * @return true always
 */
bool enableRetry(void*) {
    retry = true;
    return retry;
}

/**
 * @brief Drains the packet queue, attempting to publish each entry via MQTT.
 *
 * Stops immediately if a publish fails (MQTT still unavailable), leaving
 * remaining packets in the queue for the next attempt.
 */
void publishQueue() {
    while (!packetQueue.empty()) {
        // publishQueue operates on raw data bytes, not full CdpPackets.
        // Reconstruct a minimal CdpPacket wrapper around the stored data.
        CdpPacket pkt;
        const std::vector<unsigned char>& front = packetQueue.front();
        pkt.data = front;

        if (quackJson(pkt) == 0) {
            packetQueue.pop();
            cdpPrintf("[PAPA] Queue size: %zu\n", packetQueue.size());
        } else {
            // MQTT unavailable — stop draining and try again later
            return;
        }
    }
}