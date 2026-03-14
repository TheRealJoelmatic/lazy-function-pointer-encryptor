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
    FuncType decryptedFunction;
    FuncType stubFunction;

    size_t size;
    unsigned int key;

    void generateKey() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> dis(1, 255);
        key = dis(gen);
    }

    void buildStub()
    {
        uintptr_t enc = reinterpret_cast<uintptr_t>(decryptedFunction) ^ key;

        unsigned char code[] =
        {
            0x48,0xB8,                          // mov rax, imm64
            0,0,0,0,0,0,0,0,                    // encrypted address

            0x48,0x35,                          // xor rax, imm32
            0,0,0,0,                            // key

            0xFF,0xE0                           // jmp rax
        };

        memcpy(&code[2], &enc, 8);
        memcpy(&code[12], &key, 4);

        void* mem = VirtualAlloc(
            nullptr,
            sizeof(code),
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE
        );

        memcpy(mem, code, sizeof(code));

        stubFunction = reinterpret_cast<FuncType>(mem);
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

    void encrypt() override
    {
        for (auto& byte : encryptedData)
            byte ^= key;

        decryptedFunction = nullptr;
    }

    void decrypt() override
    {
        std::memcpy(&decryptedFunction, encryptedData.data(), size);

        for (size_t i = 0; i < size; ++i)
            ((uint8_t*)&decryptedFunction)[i] ^= key;

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
