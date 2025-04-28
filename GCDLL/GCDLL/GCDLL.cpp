/**
 * GCDLL.cpp - Implementation of encryption/decryption library for game packets
 * Compatible with CryptLib.pas interface
 */

#include "pch.h"
#include <windows.h>
#include <string>
#include <vector>
#include <ctime>
#include <random>
#include <memory>

 // Export directives
#define DLL_EXPORT extern "C" __declspec(dllexport)

// Random number generator setup
std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));

// XOR cipher key
const unsigned char DEFAULT_KEY[] = {
    0x8A, 0x45, 0xC3, 0xF7, 0x21, 0x9D, 0x63, 0xA8,
    0x5E, 0xB2, 0x7F, 0x1C, 0xD9, 0x36, 0x84, 0x0F
};

// Global memory manager to prevent memory leaks
class MemoryManager {
private:
    std::vector<char*> allocatedBuffers;

public:
    char* AllocateBuffer(size_t size) {
        char* buffer = new char[size];
        allocatedBuffers.push_back(buffer);
        return buffer;
    }

    void ClearOldBuffers() {
        // Keep only the last 10 buffers to prevent excessive memory usage
        while (allocatedBuffers.size() > 10) {
            delete[] allocatedBuffers.front();
            allocatedBuffers.erase(allocatedBuffers.begin());
        }
    }

    ~MemoryManager() {
        for (auto buffer : allocatedBuffers) {
            delete[] buffer;
        }
        allocatedBuffers.clear();
    }
};

// Global memory manager instance
MemoryManager g_MemoryManager;

// Initialize the DLL
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {
    case DLL_PROCESS_ATTACH:
        // Initialize random number generator with current time
        rng.seed(static_cast<unsigned int>(time(nullptr)));
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Helper functions for AnsiString conversion
std::vector<unsigned char> AnsiStringToVector(const char* data, size_t length) {
    if (data == nullptr || length == 0) {
        return std::vector<unsigned char>();
    }

    std::vector<unsigned char> result(length);
    memcpy(result.data(), data, length);
    return result;
}

char* VectorToAnsiString(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        // Return empty string for empty data
        char* emptyBuffer = g_MemoryManager.AllocateBuffer(1);
        emptyBuffer[0] = '\0';
        return emptyBuffer;
    }

    char* result = g_MemoryManager.AllocateBuffer(data.size() + 1);
    memcpy(result, data.data(), data.size());
    result[data.size()] = '\0'; // Null-terminate for safety

    // Clear older buffers to prevent memory growth
    g_MemoryManager.ClearOldBuffers();

    return result;
}

// Main encryption function
std::vector<unsigned char> EncryptData(const std::vector<unsigned char>& data,
    const std::vector<unsigned char>& iv,
    unsigned char randomSeed) {
    if (data.empty() || iv.empty()) {
        return std::vector<unsigned char>();
    }

    // Create a key based on IV and random seed
    std::vector<unsigned char> key(16);
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = DEFAULT_KEY[i % sizeof(DEFAULT_KEY)] ^
            iv[i % iv.size()] ^
            (randomSeed + static_cast<unsigned char>(i));
    }

    // Encrypt the data
    std::vector<unsigned char> result(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        result[i] = data[i] ^ key[i % key.size()];
    }

    return result;
}

// Main decryption function
std::vector<unsigned char> DecryptData(const std::vector<unsigned char>& data,
    const std::vector<unsigned char>& iv) {
    if (data.empty() || iv.empty()) {
        return std::vector<unsigned char>();
    }

    // Extract random seed from first byte
    unsigned char randomSeed = data[0];

    // Create the key based on IV and random seed
    std::vector<unsigned char> key(16);
    for (size_t i = 0; i < key.size(); i++) {
        key[i] = DEFAULT_KEY[i % sizeof(DEFAULT_KEY)] ^
            iv[i % iv.size()] ^
            (randomSeed + static_cast<unsigned char>(i));
    }

    // Decrypt the data
    std::vector<unsigned char> result(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        result[i] = data[i] ^ key[i % key.size()];
    }

    return result;
}

// Generate an IV based on the provided hash and type
std::vector<unsigned char> GenerateIVVector(const std::vector<unsigned char>& ivHash, DWORD ivType) {
    if (ivHash.empty()) {
        return std::vector<unsigned char>(16, 0); // Return zeroed IV if hash is empty
    }

    std::vector<unsigned char> result(16);  // 16-byte IV

    // Use different generation strategies based on the IV type
    switch (ivType) {
    case 0: // Simple hash derivation
        for (size_t i = 0; i < result.size(); i++) {
            result[i] = (ivHash[i % ivHash.size()] ^ DEFAULT_KEY[i % sizeof(DEFAULT_KEY)]);
        }
        break;

    case 1: // Time-based IV
    {
        SYSTEMTIME st;
        GetSystemTime(&st);

        // Mix the time with the hash
        result[0] = static_cast<unsigned char>(st.wYear & 0xFF);
        result[1] = static_cast<unsigned char>((st.wYear >> 8) & 0xFF);
        result[2] = static_cast<unsigned char>(st.wMonth);
        result[3] = static_cast<unsigned char>(st.wDay);
        result[4] = static_cast<unsigned char>(st.wHour);
        result[5] = static_cast<unsigned char>(st.wMinute);
        result[6] = static_cast<unsigned char>(st.wSecond);
        result[7] = static_cast<unsigned char>(st.wMilliseconds & 0xFF);

        // Mix with hash values
        for (size_t i = 0; i < result.size(); i++) {
            result[i] ^= ivHash[i % ivHash.size()];
        }
    }
    break;

    case 2: // Random IV
    {
        std::uniform_int_distribution<unsigned int> dist(0, 255);
        for (size_t i = 0; i < result.size(); i++) {
            result[i] = static_cast<unsigned char>(dist(rng));
            // Mix with hash
            result[i] ^= ivHash[i % ivHash.size()];
        }
    }
    break;

    default: // Fallback to a simple mixing
        for (size_t i = 0; i < result.size(); i++) {
            result[i] = static_cast<unsigned char>((ivHash[i % ivHash.size()] + i) ^ DEFAULT_KEY[i % sizeof(DEFAULT_KEY)]);
        }
    }

    return result;
}

// Function to prepare packet for sending
std::vector<unsigned char> PreparePacketForSending(const std::vector<unsigned char>& data,
    const std::vector<unsigned char>& iv2) {
    if (data.empty()) {
        return std::vector<unsigned char>();
    }

    // Create a copy of the input data
    std::vector<unsigned char> result = data;

    // Calculate checksum
    unsigned char checksum = 0;
    for (size_t i = 0; i < data.size(); i++) {
        checksum ^= data[i];
    }

    // Mix checksum with IV2 if provided
    if (!iv2.empty()) {
        for (size_t i = 0; i < iv2.size(); i++) {
            checksum ^= iv2[i];
        }
    }

    // Add the checksum to the end of the packet
    result.push_back(checksum);

    return result;
}

// ------ Exported Functions ------

// _Encrypt function - Encrypts packet data
DLL_EXPORT char* __stdcall _Encrypt(const char* data, const char* iv, unsigned char rnd) {
    if (data == nullptr || iv == nullptr) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Get lengths of input strings
    size_t dataLen = strlen(data);
    size_t ivLen = strlen(iv);

    if (dataLen == 0 || ivLen == 0) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Convert AnsiString parameters to vectors
    std::vector<unsigned char> dataVec = AnsiStringToVector(data, dataLen);
    std::vector<unsigned char> ivVec = AnsiStringToVector(iv, ivLen);

    // Add random seed to the beginning of data
    dataVec.insert(dataVec.begin(), rnd);

    // Encrypt the data
    std::vector<unsigned char> encryptedData = EncryptData(dataVec, ivVec, rnd);

    // Convert back to AnsiString
    return VectorToAnsiString(encryptedData);
}

// _Decrypt function - Decrypts packet data
DLL_EXPORT char* __stdcall _Decrypt(const char* data, const char* iv) {
    if (data == nullptr || iv == nullptr) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Get lengths of input strings
    size_t dataLen = strlen(data);
    size_t ivLen = strlen(iv);

    if (dataLen == 0 || ivLen == 0) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Convert AnsiString parameters to vectors
    std::vector<unsigned char> dataVec = AnsiStringToVector(data, dataLen);
    std::vector<unsigned char> ivVec = AnsiStringToVector(iv, ivLen);

    // Decrypt the data
    std::vector<unsigned char> decryptedData = DecryptData(dataVec, ivVec);

    // Remove the random seed byte if data is not empty
    if (!decryptedData.empty()) {
        decryptedData.erase(decryptedData.begin());
    }

    // Convert back to AnsiString
    return VectorToAnsiString(decryptedData);
}

// _GenerateIV function - Generates an initialization vector
DLL_EXPORT char* __stdcall _GenerateIV(const char* ivHash, DWORD ivType) {
    if (ivHash == nullptr) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Get length of input string
    size_t hashLen = strlen(ivHash);

    // Convert AnsiString parameters to vectors
    std::vector<unsigned char> ivHashVec = AnsiStringToVector(ivHash, hashLen);

    // Generate the IV
    std::vector<unsigned char> generatedIV = GenerateIVVector(ivHashVec, ivType);

    // Convert back to AnsiString
    return VectorToAnsiString(generatedIV);
}

// _ClearPacket function - Prepares packet for sending
DLL_EXPORT char* __stdcall _ClearPacket(const char* data, const char* iv2) {
    if (data == nullptr) {
        return VectorToAnsiString(std::vector<unsigned char>());
    }

    // Get lengths of input strings
    size_t dataLen = strlen(data);
    size_t iv2Len = (iv2 != nullptr) ? strlen(iv2) : 0;

    // Convert AnsiString parameters to vectors
    std::vector<unsigned char> dataVec = AnsiStringToVector(data, dataLen);
    std::vector<unsigned char> iv2Vec;

    if (iv2 != nullptr && iv2Len > 0) {
        iv2Vec = AnsiStringToVector(iv2, iv2Len);
    }

    // Prepare the packet
    std::vector<unsigned char> preparedPacket = PreparePacketForSending(dataVec, iv2Vec);

    // Convert back to AnsiString
    return VectorToAnsiString(preparedPacket);
}