//
// Created by Arno Puder on 2/5/2019.
//

#pragma once

#include <inttypes.h>
#include <string.h>
#include <assert.h>

#define TRS_IO_PORT 31

#define TRS_IO_CORE_MODULE_ID 0

#define TRS_IO_SEND_VERSION 0
#define TRS_IO_SEND_STATUS 1
#define TRS_IO_CMD_CONFIGURE_WIFI 2
#define TRS_IO_SEND_WIFI_SSID 3
#define TRS_IO_SEND_WIFI_IP 4

#define TRS_IO_MAX_MODULES 6
#define TRS_IO_MAX_COMMANDS 15
#define TRS_IO_MAX_RECEIVE_BUFFER (48 * 1024)
#define TRS_IO_MAX_SEND_BUFFER (48 * 1024)
#define TRS_IO_MAX_PARAMETERS_PER_TYPE 5

extern "C" void init_trs_io();

class TrsIO;

typedef void (TrsIO::*cmd_t)();

typedef struct {
    TrsIO* mod;
    cmd_t cmd;
    const char* signature;
} command_t;

enum state_t {
    STATE_NEXT_MODULE,
    STATE_NEXT_CMD,
    STATE_ACCEPT_FIXED_LENGTH_PARAM,
    STATE_ACCEPT_STRING_PARAM,
    STATE_ACCEPT_BLOB_LEN,
    STATE_SEND
};

class TrsIO {
private:
    static uint8_t receiveBuffer[];
    static uint8_t* receivePtr;

    static uint8_t sendBuffer[];
    static uint8_t* sendPtr;
    static uint8_t* nextByteToSend;

    static TrsIO* modules[];
    static TrsIO* currentModule;

    static uint8_t cmd;
    static state_t state;
    static uint32_t bytesToRead;

    static const char* signatureParams;

    static uint8_t* paramBytes[];
    static uint8_t* paramInts[];
    static uint8_t* paramLongs[];
    static uint8_t* paramStrs[];
    static uint8_t* paramBlobs16[];
    static uint16_t paramBlobs16Len[];
    static uint8_t* paramBlobs32[];
    static uint32_t paramBlobs32Len[];

    static uint16_t* blob16;
    static uint32_t* blob32;

    static uint16_t numParamByte;
    static uint16_t numParamInt;
    static uint16_t numParamLong;
    static uint16_t numParamStr;
    static uint16_t numParamBlob16;
    static uint16_t numParamBlob32;

    int id;
    uint16_t numCommands;
    command_t commands[TRS_IO_MAX_COMMANDS];

    static bool deferDoneFlag;

protected:
    void addCommand(cmd_t proc, const char* signature) {
        assert(numCommands != TRS_IO_MAX_COMMANDS);
        commands[numCommands].mod = this;
        commands[numCommands].cmd = proc;
        commands[numCommands].signature = signature;
        numCommands++;
    }

    void deferDone() {
      deferDoneFlag = true;
    }

public:

    explicit TrsIO(int id) {
        this->id = id;
        assert(id < TRS_IO_MAX_MODULES);
        assert(modules[id] == nullptr);
        modules[id] = this;
        numCommands = 0;
    }

    static void init();

    inline static TrsIO* getModule(int id) {
        assert(id < TRS_IO_MAX_MODULES);
        currentModule = modules[id];
        return currentModule;
    }

    virtual void initModule() {
        // Do nothing in base class
    }

    static void reset();
    static bool outZ80(uint8_t byte);
    static uint8_t inZ80();

protected:
    inline static uint8_t B(uint8_t idx) {
        assert(idx < numParamByte);
        return *paramBytes[idx];
    }

    inline static uint16_t I(uint8_t idx) {
        assert(idx < numParamInt);
        return *((uint16_t *) paramInts[idx]);
    }

    inline static uint32_t L(uint8_t idx) {
        assert(idx < numParamLong);
        return *((uint32_t *) paramLongs[idx]);
    }

    inline static const char* S(uint8_t idx) {
        assert(idx < numParamStr);
        return (const char*) paramStrs[idx];
    }

    inline static uint8_t* Z(uint8_t idx) {
        assert(idx < numParamBlob16);
        return paramBlobs16[idx];
    }

    inline static uint16_t ZL(uint8_t idx) {
        assert(idx < numParamBlob16);
        return paramBlobs16Len[idx];
    }

    inline static uint8_t* X(uint8_t idx) {
        assert(idx < numParamBlob32);
        return paramBlobs32[idx];
    }

    inline static uint32_t XL(uint8_t idx) {
        assert(idx < numParamBlob32);
        return paramBlobs32Len[idx];
    }

    inline static void rewind() {
        sendPtr = sendBuffer;
        blob16 = nullptr;
        blob32 = nullptr;
    }

    inline static void addByte(uint8_t b) {
        assert(sendPtr + sizeof(uint8_t) <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        *sendPtr++ = b;
    }

    inline static void addInt(uint16_t i) {
        assert(sendPtr + sizeof(uint16_t) <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        *((uint16_t*) sendPtr) = i;
        sendPtr += sizeof(uint16_t);
    }

    inline static void addLong(uint32_t l) {
        assert(sendPtr + sizeof(uint32_t) <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        *((uint32_t*) sendPtr) = l;
        sendPtr += sizeof(uint32_t);
    }

    inline static void addStr(const char* str) {
        assert(sendPtr + strlen(str) < sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        do {
            addByte((uint8_t) *str);
        } while (*str++ != '\0');
    }

    inline static void addBlob16(void *blob, uint16_t len) {
        assert(sendPtr + sizeof(uint16_t) + len <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        auto p = (uint8_t*) blob;
        addInt(len);
        for (uint16_t i = 0; i < len; i++) {
            addByte(*p++);
        }
    }

    inline static void addBlob32(void *blob, uint32_t len) {
        assert(sendPtr + sizeof(uint32_t) + len <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        auto p = (uint8_t*) blob;
        addLong(len);
        for (uint32_t i = 0; i < len; i++) {
            addByte(*p++);
        }
    }

    inline static void skip(uint32_t len) {
        assert(sendPtr + len <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        sendPtr += len;
    }

    inline static uint8_t* startBlob16() {
        assert(sendPtr + sizeof(uint16_t) <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        assert(blob16 == nullptr);
        blob16 = (uint16_t*) sendPtr;
        sendPtr += sizeof(uint16_t);
        return sendPtr;
    }

    inline static void endBlob16() {
        assert(blob16 != nullptr);
        *blob16 = (uint16_t) (sendPtr - ((uint8_t*) blob16) - sizeof(uint16_t));
        blob16 = nullptr;
    }

    inline static uint8_t* startBlob32() {
        assert(sendPtr + sizeof(uint32_t) <= sendBuffer + TRS_IO_MAX_SEND_BUFFER);
        assert(blob32 == nullptr);
        blob32 = (uint32_t*) sendPtr;
        sendPtr += sizeof(uint32_t);
        return sendPtr;
    }

    inline static void endBlob32() {
        assert(blob32 != nullptr);
        *blob32 = (uint32_t) (sendPtr - ((uint8_t*) blob32) - sizeof(uint32_t));
        blob32 = nullptr;
    }

    void process();

public:
    static bool processInBackground();
    static unsigned long getSendBufferFreeSize() {
        return (sendBuffer + TRS_IO_MAX_SEND_BUFFER) - sendPtr;
    }
};
