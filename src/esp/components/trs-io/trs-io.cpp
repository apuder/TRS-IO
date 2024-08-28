
#include "trs-io.h"
#include "esp_attr.h"

using namespace std;

uint8_t TrsIO::sendBuffer[TRS_IO_MAX_SEND_BUFFER] EXT_RAM_ATTR;
uint8_t* TrsIO::sendPtr;
uint8_t* TrsIO::nextByteToSend;

uint8_t TrsIO::receiveBuffer[TRS_IO_MAX_RECEIVE_BUFFER] EXT_RAM_ATTR;
uint8_t* TrsIO::receivePtr;

TrsIO* TrsIO::modules[TRS_IO_MAX_MODULES];
TrsIO* TrsIO::currentModule;

uint8_t TrsIO::cmd;
state_t TrsIO::state;
uint32_t TrsIO::bytesToRead;

const char* TrsIO::signatureParams;

uint8_t* TrsIO::paramBytes[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramInts[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramLongs[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramStrs[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramBlobs16[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint16_t TrsIO::paramBlobs16Len[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint8_t* TrsIO::paramBlobs32[TRS_IO_MAX_PARAMETERS_PER_TYPE];
uint32_t TrsIO::paramBlobs32Len[TRS_IO_MAX_PARAMETERS_PER_TYPE];

uint16_t* TrsIO::blob16;
uint32_t* TrsIO::blob32;

uint16_t TrsIO::numParamByte;
uint16_t TrsIO::numParamInt;
uint16_t TrsIO::numParamLong;
uint16_t TrsIO::numParamStr;
uint16_t TrsIO::numParamBlob16;
uint16_t TrsIO::numParamBlob32;

bool TrsIO::deferDoneFlag;

extern "C" {
    void init_trs_io() {
        TrsIO::init();
    }
}

void TrsIO::init() {
    for (auto &module : modules) {
        TrsIO* mod = module;
        if (mod != nullptr) {
            module->initModule();
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
    numParamBlob16 = 0;
    numParamBlob32 = 0;
    blob16 = nullptr;
    blob32 = nullptr;
    receivePtr = receiveBuffer;
}

bool TrsIO::outZ80(uint8_t byte) {
    static union {
        uint8_t b[sizeof(uint32_t)];
        uint16_t i;
        uint32_t l;
    } len;
    static uint8_t count;

    switch (state) {
        case STATE_NEXT_MODULE:
            if (byte >= TRS_IO_MAX_MODULES || modules[byte] == nullptr) {
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
        case STATE_ACCEPT_BLOB_LEN:
            len.b[count++] = byte;
            if (count == bytesToRead) {
                if (bytesToRead == sizeof(uint16_t)) {
                    bytesToRead = len.i;
                    paramBlobs16Len[numParamBlob16] = len.i;
                    paramBlobs16[numParamBlob16++] = receivePtr;
                } else {
                    bytesToRead = len.l;
                    paramBlobs32Len[numParamBlob32] = len.l;
                    paramBlobs32[numParamBlob32++] = receivePtr;
                }
                state = STATE_ACCEPT_FIXED_LENGTH_PARAM;
            }
            return true;
        case STATE_SEND:
            reset();
            return outZ80(byte);
    }

    if (*signatureParams == '\0') {
        state = STATE_SEND;
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
            bytesToRead = sizeof(uint16_t);
            count = 0;
            state = STATE_ACCEPT_BLOB_LEN;
            break;
        case 'X':
            bytesToRead = sizeof(uint32_t);
            count = 0;
            state = STATE_ACCEPT_BLOB_LEN;
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

bool TrsIO::processInBackground() {
  deferDoneFlag = false;
  currentModule->process();
  nextByteToSend = sendBuffer;
  return deferDoneFlag;
}

void TrsIO::process() {
    sendPtr = sendBuffer;
    TrsIO* mod = commands[cmd].mod;
    cmd_t p = commands[cmd].cmd;
    (mod->*(p))();
}
