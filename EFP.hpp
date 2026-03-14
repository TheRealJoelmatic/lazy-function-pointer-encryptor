#pragma once

#include <vector>
#include <random>
#include <memory>
#include <cstring>
#include <type_traits>
#include <windows.h>

class EncryptedFunctionBase {
public:
    virtual ~EncryptedFunctionBase() = default;
    virtual void decrypt() = 0;
    virtual void encrypt() = 0;
};

template <typename Ret, typename... Args>
class EncryptedFunction : public EncryptedFunctionBase {
private:

    using FuncType = Ret(*)(Args...);

    std::vector<uint8_t> encryptedData;
    FuncType decryptedFunction = nullptr;
    FuncType stubFunction = nullptr;

    size_t size;
    uint64_t key;
    bool stubBuilt = false;

    void generateKey() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint64_t> dis(1, 0xFFFFFFFFFFFFFFFF);
        key = dis(gen);
    }

    void buildStub()
    {
        if (stubBuilt)
            return;

        uintptr_t enc = reinterpret_cast<uintptr_t>(decryptedFunction) ^ key;

        unsigned char code[] =
        {
            0x48,0xB8,                          // mov rax, imm64
            0,0,0,0,0,0,0,0,                    // encrypted pointer

            0x48,0xBA,                          // mov rdx, imm64
            0,0,0,0,0,0,0,0,                    // key

            0x48,0x31,0xD0,                     // xor rax, rdx
            0xFF,0xE0                           // jmp rax
        };

        memcpy(&code[2], &enc, 8);
        memcpy(&code[12], &key, 8);

        void* mem = VirtualAlloc(
            nullptr,
            sizeof(code),
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        memcpy(mem, code, sizeof(code));

        stubFunction = reinterpret_cast<FuncType>(mem);
        stubBuilt = true;
    }

public:

    EncryptedFunction(FuncType func)
    {
        generateKey();

        size = sizeof(FuncType);
        encryptedData.resize(size);

        std::memcpy(encryptedData.data(), &func, size);

        encrypt();
    }

    ~EncryptedFunction()
    {
        if (stubFunction)
            VirtualFree((void*)stubFunction, 0, MEM_RELEASE);
    }

    void encrypt() override
    {
        for (auto& byte : encryptedData)
            byte ^= (uint8_t)key;

        decryptedFunction = nullptr;
    }

    void decrypt() override
    {
        std::memcpy(&decryptedFunction, encryptedData.data(), size);

        for (size_t i = 0; i < size; ++i)
            ((uint8_t*)&decryptedFunction)[i] ^= (uint8_t)key;

        buildStub();
    }

    Ret operator()(Args... args)
    {
        decrypt();

        if constexpr (std::is_void_v<Ret>)
        {
            stubFunction(args...);
            encrypt();
        }
        else
        {
            Ret result = stubFunction(args...);
            encrypt();
            return result;
        }
    }
};
