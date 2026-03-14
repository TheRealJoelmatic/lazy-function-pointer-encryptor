# lazy-function-pointer-encryptor

A lightweight, header-only C++ library for encrypting function pointers and decrypting them at runtime. Designed for runtime obfuscation and resistance to static analysis.

---

## Overview

`lazy-function-pointer-encryptor` wraps any function pointer in an encrypted container. The pointer is XOR-encrypted with a randomly generated 64-bit key at construction time and is only decrypted immediately before a call is made. After the call completes the pointer is re-encrypted, so the plaintext address is never resident in memory for longer than necessary.

The decryption and dispatch is handled through a small x64 assembly stub allocated in executable memory. The stub loads the encrypted address and the full 64-bit key into registers, XORs them together, and jumps to the result. The stub is built once and reused across calls, and the allocated memory is freed when the `EncryptedFunction` object is destroyed.

![Preview](images/Screenshot%202026-03-14%20232521.png)

---

## Requirements

- Windows (uses `VirtualAlloc` / `VirtualFree` and `PAGE_EXECUTE_READWRITE`)
- 64-bit target architecture (x86-64)

---

## Installation

The library is a single header file. Copy `EFP.hpp` into your project and include it.

```cpp
#include "EFP.hpp"
```

---

## Usage

### Basic example

```cpp
#include "EFP.hpp"
#include <iostream>

int Add(int a, int b) {
    return a + b;
}

void PrintMessage(const char* msg) {
    std::cout << msg << "\n";
}

// Wrap a function that returns int and takes two ints
EncryptedFunction<int, int, int> encAdd(&Add);

// Wrap a void function
EncryptedFunction<void, const char*> encPrint(&PrintMessage);

int main() {
    // Call it exactly like the original function
    int result = encAdd(3, 7);  // returns 10
    encPrint("Hello from an encrypted function pointer");

    return 0;
}
```

### Template parameters

```
EncryptedFunction<ReturnType, Arg1Type, Arg2Type, ...>
```

| Parameter | Description |
|-----------|-------------|
| `ReturnType` | The return type of the wrapped function. Use `void` for functions that return nothing. |
| `Arg1Type, ...` | The types of each argument the function accepts, in order. |

---

## How it works

1. **Construction** - The constructor stores the raw function pointer bytes, generates a random 64-bit XOR key (1 through 0xFFFFFFFFFFFFFFFF, inclusive), and immediately XOR-encrypts the stored bytes using the low byte of the key. The plaintext pointer is discarded.

2. **Call** - When `operator()` is invoked, `decrypt()` XOR-decrypts the stored bytes back into a temporary `FuncType` value and calls `buildStub()`.

3. **Stub generation** - `buildStub()` runs only once per instance. It allocates a small region of executable memory via `VirtualAlloc` and writes a 23-byte x64 sequence into it:
   ```cpp
        unsigned char code[] =
        {
            0x48,0xB8,                          // mov rax, imm64
            0,0,0,0,0,0,0,0,                    // encrypted pointer

            0x48,0xBA,                          // mov rdx, imm64
            0,0,0,0,0,0,0,0,                    // key

            0x48,0x31,0xD0,                     // xor rax, rdx
            0xFF,0xE0                           // jmp rax
        };
   ```
   Both the encrypted pointer and the full 64-bit key are embedded directly in the instruction stream. The real target address is only materialized inside the CPU at the moment of execution.

4. **Dispatch** - The stub is called with the original arguments. For non-void return types the result is saved before continuing.

5. **Re-encryption** - After the call returns, `encrypt()` XORs the stored bytes again, leaving no plaintext pointer in memory.

6. **Destruction** - The destructor calls `VirtualFree` to release the stub's executable memory.

---

## API reference

### `EncryptedFunction<Ret, Args...>`

**Constructor**

```cpp
EncryptedFunction(Ret(*func)(Args...))
```

Stores and immediately encrypts `func`. A new random 64-bit key is generated for each instance. The executable stub is not built until the first call.

**Destructor**

```cpp
~EncryptedFunction()
```

Frees the executable stub memory allocated by `VirtualAlloc`.

**Call operator**

```cpp
Ret operator()(Args... args)
```

Decrypts the pointer, calls the function through the generated stub, re-encrypts, and returns the result. Transparent drop-in replacement for a direct function call. The stub is built on the first call and reused on subsequent calls.

**`void encrypt()`**

XOR-encrypts the stored pointer bytes using the low byte of the instance key and sets the internal decrypted pointer to `nullptr`.

**`void decrypt()`**

XOR-decrypts the stored pointer bytes into a temporary value and builds the executable call stub if it has not been built yet.

---

## Notes

- XOR encryption is intended for obfuscation only. It is not a cryptographically secure scheme.
- The in-memory storage (`encryptedData`) is encrypted byte-by-byte using only the low byte of the 64-bit key. This provides a simple layer of obfuscation against memory dumps. The stub uses the full 64-bit key for its inline `xor rax, rdx` so that a static view of the stub's instruction stream does not directly reveal the real target address.
- The encryption key is stored alongside the encrypted data in the same object. A determined analyst with access to process memory can reconstruct the original pointer.
- The executable stub is built once and reused across all calls.
- The assembly stub is hardcoded for the x86-64 calling convention and will not work on 32-bit targets.

---
