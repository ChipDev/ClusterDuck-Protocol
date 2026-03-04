/**
 * @file WiFiClientSecure.h
 * @brief Linux drop-in for Arduino's WiFiClientSecure.
 *
 * On Arduino, WiFiClientSecure owns both the TLS socket and the credentials.
 * On Linux, Paho owns the socket; this class is a pure credential store whose
 * contents are read by PubSubClient when it builds its mqtt::ssl_options.
 *
 * Usage mirrors the Arduino API exactly:
 *   WiFiClientSecure wifiClient;
 *   wifiClient.setCACert(AWS_CERT_CA);
 *   wifiClient.setCertificate(AWS_CERT_CRT);
 *   wifiClient.setPrivateKey(AWS_CERT_PRIVATE);
 */

#pragma once
#include <string>

class WiFiClientSecure {
public:
    WiFiClientSecure() = default;

    /** PEM-encoded CA certificate (trust anchor). */
    void setCACert(const char* pem)     { caCert_     = pem ? pem : ""; }

    /** PEM-encoded client certificate. */
    void setCertificate(const char* pem){ certificate_ = pem ? pem : ""; }

    /** PEM-encoded private key matching the client certificate. */
    void setPrivateKey(const char* pem) { privateKey_  = pem ? pem : ""; }

    // --- Accessors used by PubSubClient ---
    const std::string& caCert()      const { return caCert_;      }
    const std::string& certificate() const { return certificate_; }
    const std::string& privateKey()  const { return privateKey_;  }

private:
    std::string caCert_;
    std::string certificate_;
    std::string privateKey_;
};