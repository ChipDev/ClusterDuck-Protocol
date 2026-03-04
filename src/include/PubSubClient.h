/**
 * @file PubSubClient.h
 * @brief Linux drop-in for Arduino's PubSubClient, backed by Eclipse Paho C++.
 *
 * Mirrors the exact subset of the Arduino PubSubClient API used by PapaDuck:
 *
 *   PubSubClient client(host, port, callbackFn, wifiClient);
 *   client.connected()
 *   client.connect(clientId)
 *   client.loop()
 *   client.publish(topic, payload)
 *   client.subscribe(topic)
 *
 * TLS credentials are read from the WiFiClientSecure object at connect() time.
 * Paho writes the CA cert, client cert, and private key to temporary files
 * because the Paho C library requires filesystem paths for SSL options.
 * Files are written to /tmp and removed when the object is destroyed.
 *
 * Build requirements:
 *   - Eclipse Paho C++  (paho.mqtt.cpp, built with PAHO_WITH_SSL=ON)
 *   - Link:  -lpaho-mqttpp3 -lpaho-mqtt3as -lssl -lcrypto
 */

#pragma once

#include <string>
#include <functional>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include <mqtt/async_client.h>

#include "WiFiClientSecure.h"

// Callback type matching the Arduino PubSubClient convention.
using MQTTCallback = std::function<void(char*, unsigned char*, unsigned int)>;

// Default QoS used for all publish and subscribe operations.
static constexpr int MQTT_QOS = 1;

// Paho connection timeout in seconds.
static constexpr int CONNECT_TIMEOUT_SEC = 10;

// ─────────────────────────────────────────────────────────────────────────────

class PubSubClient {
public:
    /**
     * @brief Construct a PubSubClient.
     *
     * @param host        AWS IoT endpoint hostname (no scheme, no port)
     * @param port        MQTT-over-TLS port (8883 for AWS IoT)
     * @param callback    Message-received callback: void(char* topic, byte* payload, unsigned int len)
     * @param wifiClient  Credential store holding CA cert, client cert, and private key
     */
    PubSubClient(const char*        host,
                 uint16_t           port,
                 MQTTCallback       callback,
                 WiFiClientSecure&  wifiClient)
        : host_(host)
        , port_(port)
        , callback_(std::move(callback))
        , creds_(wifiClient)
        , pahoClient_(nullptr)
    {}

    ~PubSubClient() {
        removeTempFiles();
    }

    // Non-copyable — Paho async_client is not copyable either.
    PubSubClient(const PubSubClient&)            = delete;
    PubSubClient& operator=(const PubSubClient&) = delete;

    // ── Arduino-compatible API ────────────────────────────────────────────────

    /**
     * @brief Returns true if the MQTT connection is currently active.
     */
    bool connected() {
        return pahoClient_ && pahoClient_->is_connected();
    }

    /**
     * @brief Connect to the broker using the supplied client ID.
     *
     * Writes TLS credentials to /tmp, builds ssl_options and connect_options,
     * then performs a synchronous blocking connect (times out after
     * CONNECT_TIMEOUT_SEC seconds).
     *
     * @param clientId  MQTT client identifier string (must be unique per AWS IoT thing)
     * @return true on success, false on any error
     */
    bool connect(const char* clientId) {
        try {
            // Build the broker URI: "ssl://host:port"
            std::string uri = "ssl://" + host_ + ":" + std::to_string(port_);

            // (Re)create the Paho async_client for this clientId / URI.
            pahoClient_ = std::make_unique<mqtt::async_client>(uri, clientId);

            // Register the message callback.
            pahoClient_->set_message_callback(
                [this](mqtt::const_message_ptr msg) {
                    if (!callback_) return;
                    const std::string& topic   = msg->get_topic();
                    const std::string& payload = msg->to_string();
                    // Cast away const to match the Arduino callback signature.
                    callback_(const_cast<char*>(topic.c_str()),
                              reinterpret_cast<unsigned char*>(
                                  const_cast<char*>(payload.data())),
                              static_cast<unsigned int>(payload.size()));
                }
            );

            // Write credentials to temp files (Paho C needs file paths).
            if (!writeTempFiles()) {
                fprintf(stderr, "[PubSubClient] Failed to write TLS temp files\n");
                return false;
            }

            // Build TLS options.
            mqtt::ssl_options sslOpts;
            sslOpts.set_trust_store(caFile_);       // CA cert  → verify broker
            sslOpts.set_key_store(certFile_);        // client cert
            sslOpts.set_private_key(keyFile_);       // client private key
            sslOpts.set_enable_server_cert_auth(true);

            // Build connect options.
            mqtt::connect_options connOpts;
            connOpts.set_ssl(sslOpts);
            connOpts.set_keep_alive_interval(60);
            connOpts.set_clean_session(true);
            connOpts.set_connect_timeout(std::chrono::seconds(CONNECT_TIMEOUT_SEC));

            // Blocking connect — wait for the token to complete.
            mqtt::token_ptr tok = pahoClient_->connect(connOpts);
            tok->wait();

            fprintf(stdout, "[PubSubClient] Connected to %s as %s\n",
                    uri.c_str(), clientId);
            return true;

        } catch (const mqtt::exception& ex) {
            fprintf(stderr, "[PubSubClient] connect() exception: %s\n", ex.what());
            return false;
        }
    }

    /**
     * @brief Service the MQTT connection (process keepalives and inbound messages).
     *
     * In Paho C++ with async_client, inbound messages are delivered via callback
     * automatically on a background thread.  This method therefore only checks
     * liveness; it returns false when the connection is lost so the caller can
     * trigger a reconnect — matching Arduino PubSubClient::loop() semantics.
     *
     * @return true if still connected, false if the connection has dropped
     */
    bool loop() {
        return connected();
    }

    /**
     * @brief Publish a null-terminated string payload to a topic.
     *
     * @param topic    MQTT topic string
     * @param payload  Null-terminated payload string
     * @return true on successful publish, false on error or disconnected
     */
    bool publish(const char* topic, const char* payload) {
        if (!connected()) return false;
        try {
            auto msg = mqtt::make_message(topic, payload, strlen(payload), MQTT_QOS, false);
            mqtt::token_ptr tok = pahoClient_->publish(msg);
            tok->wait();
            return true;
        } catch (const mqtt::exception& ex) {
            fprintf(stderr, "[PubSubClient] publish() exception: %s\n", ex.what());
            return false;
        }
    }

    /**
     * @brief Subscribe to a topic (supports MQTT wildcards: + and #).
     *
     * @param topic  MQTT topic filter string
     * @return true on successful subscribe, false on error or disconnected
     */
    bool subscribe(const char* topic) {
        if (!connected()) return false;
        try {
            mqtt::token_ptr tok = pahoClient_->subscribe(topic, MQTT_QOS);
            tok->wait();
            return true;
        } catch (const mqtt::exception& ex) {
            fprintf(stderr, "[PubSubClient] subscribe() exception: %s\n", ex.what());
            return false;
        }
    }

private:
    // ── Helpers ──────────────────────────────────────────────────────────────

    /**
     * @brief Write PEM strings from WiFiClientSecure to unique /tmp files.
     *
     * Paho's underlying C library (libpaho-mqtt3as) needs filesystem paths
     * for SSL configuration rather than in-memory PEM strings.
     *
     * @return true if all three files were written successfully
     */
    bool writeTempFiles() {
        removeTempFiles(); // Clean up any leftovers from a previous connect attempt.

        caFile_   = writePem("/tmp/papaduck_ca_XXXXXX.pem",   creds_.caCert());
        certFile_ = writePem("/tmp/papaduck_cert_XXXXXX.pem", creds_.certificate());
        keyFile_  = writePem("/tmp/papaduck_key_XXXXXX.pem",  creds_.privateKey());

        return !caFile_.empty() && !certFile_.empty() && !keyFile_.empty();
    }

    /**
     * @brief Write a PEM string to a uniquely-named temp file.
     *
     * Uses mkstemps() so the file has a .pem extension, which some OpenSSL
     * versions require to recognise the format.
     *
     * @param tmpl  Path template with XXXXXX placeholder and .pem suffix
     * @param pem   PEM string to write
     * @return Resolved file path on success, empty string on failure
     */
    static std::string writePem(const char* tmpl, const std::string& pem) {
        if (pem.empty()) {
            fprintf(stderr, "[PubSubClient] writePem: empty PEM string for %s\n", tmpl);
            return "";
        }

        // mkstemps requires a mutable buffer and the suffix length (.pem = 4).
        char path[256];
        snprintf(path, sizeof(path), "%s", tmpl);
        int fd = mkstemps(path, 4 /* len of ".pem" */);
        if (fd < 0) {
            perror("[PubSubClient] mkstemps");
            return "";
        }

        ssize_t written = write(fd, pem.c_str(), pem.size());
        close(fd);

        if (written != static_cast<ssize_t>(pem.size())) {
            fprintf(stderr, "[PubSubClient] writePem: short write for %s\n", path);
            unlink(path);
            return "";
        }

        return std::string(path);
    }

    /** Remove the three temp PEM files if they exist. */
    void removeTempFiles() {
        for (const std::string* f : {&caFile_, &certFile_, &keyFile_}) {
            if (!f->empty()) unlink(f->c_str());
        }
        caFile_ = certFile_ = keyFile_ = "";
    }

    // ── Member data ──────────────────────────────────────────────────────────

    std::string                         host_;
    uint16_t                            port_;
    MQTTCallback                        callback_;
    WiFiClientSecure&                   creds_;
    std::unique_ptr<mqtt::async_client> pahoClient_;

    // Paths of the temporary PEM files on disk.
    std::string caFile_;
    std::string certFile_;
    std::string keyFile_;
};