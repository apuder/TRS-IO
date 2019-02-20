
#include "trs-io.h"


using namespace std;

uint8_t TrsIO::sendBuffer[TRSIO_MAX_SEND_BUFFER];
uint8_t* TrsIO::sendPtr;
uint8_t* TrsIO::nextByteToSend;

uint8_t TrsIO::receiveBuffer[TRSIO_MAX_RECEIVE_BUFFER];
uint8_t* TrsIO::receivePtr;

TrsIO* TrsIO::modules[TRSIO_MAX_MODULES];
TrsIO* TrsIO::currentModule;

uint8_t TrsIO::cmd;
state_t TrsIO::state;
uint16_t TrsIO::bytesToRead;

const char* TrsIO::signatureParams;

uint8_t* TrsIO::paramBytes[TRSIO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramInts[TRSIO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramLongs[TRSIO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramStrs[TRSIO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramBlobs[TRSIO_MAX_PARAMETERS_PER_TYPE];
uint16_t TrsIO::paramBlobsLen[TRSIO_MAX_PARAMETERS_PER_TYPE];

uint16_t* TrsIO::blob;

uint16_t TrsIO::numParamByte;
uint16_t TrsIO::numParamInt;
uint16_t TrsIO::numParamLong;
uint16_t TrsIO::numParamStr;
uint16_t TrsIO::numParamBlob;

void TrsIO::initModules() {
    for (auto &module : modules) {
        TrsIO* mod = module;
        if (mod != nullptr) {
            module->init();
        }
    }
    reset();
}

void TrsIO::reset() {
    state = STATE_NEXT_MODULE;
    numParamByte = 0;
    numParamInt = 0;
    numParamLong = 0;
    numParamStr = 0;
    numParamBlob = 0;
    receivePtr = receiveBuffer;
    blob = nullptr;
}

bool TrsIO::outZ80(uint8_t byte) {
    static uint8_t last_byte;

    switch (state) {
        case STATE_NEXT_MODULE:
            if (byte >= TRSIO_MAX_MODULES || modules[byte] == nullptr) {
                return true;
            }
            currentModule = modules[byte];
            state = STATE_NEXT_CMD;
            return true;
        case STATE_NEXT_CMD:
            if (byte >= currentModule->numCommands) {
                state = STATE_NEXT_MODULE;
                return true;
            }
            cmd = byte;
            signatureParams = currentModule->commands[cmd].signature;
            break;
        case STATE_ACCEPT_FIXED_LENGTH_PARAM:
            *receivePtr++ = byte;
            if (--bytesToRead == 0) {
                break;
            }
            return true;
        case STATE_ACCEPT_STRING_PARAM:
            *receivePtr++ = byte;
            if (byte == 0) {
                break;
            }
            return true;
        case STATE_ACCEPT_BLOCK_LEN_1:
            last_byte = byte;
            state = STATE_ACCEPT_BLOCK_LEN_2;
            return true;
        case STATE_ACCEPT_BLOCK_LEN_2:
            bytesToRead = last_byte | (byte << 8);
            paramBlobsLen[numParamBlob] = bytesToRead;
            paramBlobs[numParamBlob++] = receivePtr;
            state = STATE_ACCEPT_FIXED_LENGTH_PARAM;
            return true;
        case STATE_SEND:
            reset();
            return outZ80(byte);
    }

    if (*signatureParams == '\0') {
        state = STATE_SEND;
        currentModule->process();
        nextByteToSend = sendBuffer;
        return false;
    }

    switch (*signatureParams++) {
        case 'B':
            paramBytes[numParamByte++] = receivePtr;
            bytesToRead = 1;
            state = STATE_ACCEPT_FIXED_LENGTH_PARAM;
            break;
        case 'I':
            paramInts[numParamInt++] = receivePtr;
            bytesToRead = 2;
            state = STATE_ACCEPT_FIXED_LENGTH_PARAM;
            break;
        case 'L':
            paramLongs[numParamLong++] = receivePtr;
            bytesToRead = 4;
            state = STATE_ACCEPT_FIXED_LENGTH_PARAM;
            break;
        case 'S':
            paramStrs[numParamStr++] = receivePtr;
            state = STATE_ACCEPT_STRING_PARAM;
            break;
        case 'Z':
            state = STATE_ACCEPT_BLOCK_LEN_1;
            break;
        default:
            // bad signature string
            assert(0);
    }
    return true;
}

uint8_t TrsIO::inZ80() {
    if (state != STATE_SEND) {
        reset();
        return 0xff;
    }
    if (nextByteToSend == sendPtr) {
        reset();
        return 0xff;
    }
    return *nextByteToSend++;
}

void TrsIO::process() {
    sendPtr = sendBuffer;
    commands[cmd].proc();
}
