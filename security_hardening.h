#ifndef SECURITY_HARDENING_H
#define SECURITY_HARDENING_H

#define SECURITY_MAX_TRANSPARENT_PAYLOAD 240
#define SECURITY_MAX_FRAME_LENGTH 320
#define SECURITY_MAX_INVALID_ATTEMPTS 5
#define SECURITY_LOCKOUT_MS 30000UL
#define SECURITY_INPUT_TIMEOUT_MS 2000UL
#define SECURITY_FRAME_HEADER_CHAR '@'
#define SECURITY_FRAME_SEPARATOR_CHAR ':'
#define SECURITY_FRAME_TAIL_CHAR '#'

enum SecurityInputSource {
  SECURITY_SOURCE_USB = 0,
  SECURITY_SOURCE_WEB = 1,
  SECURITY_SOURCE_TCP_CLIENT = 2,
  SECURITY_SOURCE_TCP_SERVER = 3
};

struct SecurityState {
  uint8_t failureCount;
  unsigned long blockUntil;
  unsigned long lastActivity;
};

extern SecurityState usbSecurityState;
extern SecurityState webSecurityState;
extern SecurityState clientTcpSecurityState;
extern SecurityState serverTcpSecurityStates[];

extern String usbFrameBuffer;
extern String webFrameBuffer;
extern String clientTcpFrameBuffer;
extern String serverTcpFrameBuffers[];

String buildSecureFrame(const String &payload);
bool sendValidatedPayloadToUART(uart_port_t uartPort, const String &payload);
bool sendFramedPayloadToClient(WiFiClient &client, const String &payload);
bool isAsciiDigitChar(char value);
bool isSourceBlocked(SecurityState &state);
void clearSecurityState(SecurityState &state);
void recordSecurityActivity(SecurityState &state);
void recordSecurityFailure(SecurityState &state);
void recordSecuritySuccess(SecurityState &state);
bool isAllowedFrameCharacter(char value);
bool isAllowedPayloadCharacter(char value);
bool isTelemetryLikePayload(const String &payload);
bool payloadMatchesWhitelist(const String &payload);
bool payloadContainsBlockedToken(const String &payload);
bool validateTransparentPayload(const String &payload, String &errorReason);
bool parseSecureFrame(const String &frame, String &payload, String &errorReason);
String *getSecurityFrameBuffer(SecurityInputSource source, int clientIndex);
SecurityState *getSecurityState(SecurityInputSource source, int clientIndex);
bool enforceSecurityTimeout(SecurityInputSource source, int clientIndex);
void refreshSecurityTimeouts();
bool appendIngressChunk(SecurityInputSource source, int clientIndex, const uint8_t *data, size_t len, String &errorReason);
bool getValidatedPayload(SecurityInputSource source, int clientIndex, String &payload, String &errorReason);
bool isSecuritySourceBlocked(SecurityInputSource source, int clientIndex);
uint8_t getSecurityFailureCount(SecurityInputSource source, int clientIndex);

#endif