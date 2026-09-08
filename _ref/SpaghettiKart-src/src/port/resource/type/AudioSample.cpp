#include "AudioSample.h"

namespace SM64 {
AudioBankSample* AudioSample::GetPointer() {
    return &mData;
}

size_t AudioSample::GetPointerSize() {
    return sizeof(mData);
}
AudioSample::~AudioSample() {
    if (mData.sampleAddr != nullptr) {
        // delete[] mData.sampleAddr;
        mData.sampleAddr = nullptr;
    }
    if (mData.book->book != nullptr) {
        delete[] mData.book->book;
        mData.book->book = nullptr;
    }
    if (mData.loop->state != nullptr) {
        delete[] mData.loop->state;
        mData.loop->state = nullptr;
    }
}
}