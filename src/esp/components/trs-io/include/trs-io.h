//
// Created by Arno Puder on 2/5/2019.
//

#pragma once

#include <inttypes.h>
#include <string.h>
#include <assert.h>

#define TRSIO_MAX_MODULES 5
#define TRSIO_MAX_COMMANDS 15
#define TRSIO_MAX_RECEIVE_BUFFER (48 * 1024)
#define TRSIO_MAX_SEND_BUFFER (48 * 1024)
#define TRSIO_MAX_PARAMETERS_PER_TYPE 5

typedef void (*proc_t)();

typedef struct {
    proc_t proc;
    const char* signature;
} command_t;

enum state_t {
    STATE_NEXT_MODULE,
    STATE_NEXT_CMD,
    STATE_ACCEPT_FIXED_LENGTH_PARAM,
    STATE_ACCEPT_STRING_PARAM,
    STATE_ACCEPT_BLOCK_LEN_1,
    STATE_ACCEPT_BLOCK_LEN_2,
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
    static uint16_t bytesToRead;

    static const char* signatureParams;

    static uint8_t* paramBytes[];
    static uint8_t* paramInts[];
    static uint8_t* paramLongs[];
    static uint8_t* paramStrs[];
    static uint8_t* paramBlobs[];
    static uint16_t paramBlobsLen[];

    static uint16_t* blob;

    static uint16_t numParamByte;
    static uint16_t numParamInt;
    static uint16_t numParamLong;
    static uint16_t numParamStr;
    static uint16_t numParamBlob;

    uint16_t numCommands;
    command_t commands[TRSIO_MAX_COMMANDS];

protected:
    void addCommand(proc_t proc, const char* signature) {
        assert(numCommands != TRSIO_MAX_COMMANDS);
        commands[numCommands].proc = proc;
        commands[numCommands].signature = signature;
        numCommands++;
    }
public:

    explicit TrsIO(int id) {
        assert(id < TRSIO_MAX_MODULES);
        assert(modules[id] == nullptr);
        modules[id] = this;
        numCommands = 0;
    }

    static void initModules();

    inline static TrsIO* getModule(int id) {
        assert(id < TRSIO_MAX_MODULES);
        currentModule = modules[id];
        return currentModule;
    }

    virtual void init() {
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
        assert(idx < numParamBlob);
        return paramBlobs[idx];
    }

    inline static uint16_t ZL(uint8_t idx) {
        assert(idx < numParamBlob);
        return paramBlobsLen[idx];
    }

    inline static void addByte(uint8_t b) {
        assert(sendPtr + sizeof(uint8_t) <= sendBuffer + TRSIO_MAX_SEND_BUFFER);
        *sendPtr++ = b;
    }

    inline static void addInt(uint16_t i) {
        assert(sendPtr + sizeof(uint16_t) <= sendBuffer + TRSIO_MAX_SEND_BUFFER);
        *((uint16_t*) sendPtr) = i;
        sendPtr += sizeof(uint16_t);
    }

    inline static void addLong(uint32_t l) {
        assert(sendPtr + sizeof(uint32_t) <= sendBuffer + TRSIO_MAX_SEND_BUFFER);
        *((uint32_t*) sendPtr) = l;
        sendPtr += sizeof(uint32_t);
    }

    inline static void addStr(const char* str) {
        assert(sendPtr + strlen(str) < sendBuffer + TRSIO_MAX_SEND_BUFFER);
        do {
            addByte((uint8_t) *str);
        } while (*str++ != '\0');
    }

    inline static void addBlob(void* blob, uint16_t len) {
        assert(sendPtr + sizeof(uint16_t) + len <= sendBuffer + TRSIO_MAX_SEND_BUFFER);
        auto p = (uint8_t*) blob;
        addInt(len);
        for (int i = 0; i < len; i++) {
            addByte(*p++);
        }
    }

    inline static void startBlob() {
        assert(sendPtr + 2 <= sendBuffer + TRSIO_MAX_SEND_BUFFER);
        assert(blob == nullptr);
        blob = (uint16_t*) sendPtr;
        sendPtr += 2;
    }

    inline static void endBlob() {
        assert(blob != nullptr);
        *blob = (uint16_t) (sendPtr - ((uint8_t*) blob) - 2);
        blob = nullptr;
    }

public:
    static unsigned long getSendBufferLen() {
        return sendPtr - sendBuffer;
    }

    static uint8_t* getSendBuffer() {
        return sendBuffer;
    }

    void process();
};
