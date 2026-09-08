#include "AudioBank.h"

namespace SM64 {
CtlEntry* AudioBank::GetPointer() {
    return &mData;
}

size_t AudioBank::GetPointerSize() {
    return sizeof(mData);
}
AudioBank::~AudioBank() {
    for (auto& instrument : instruments) {
        if (instrument != nullptr) {
            if (instrument->envelope != nullptr) {
                delete[] instrument->envelope;
                instrument->envelope = nullptr;
            }
            delete instrument;
        }
        
    }
    for (auto& drum : drums) {
        delete drum;
    }
    instruments.clear();
    drums.clear();
}
}