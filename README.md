# lazy-function-pointer-encryptor

A lightweight, header-only C++ library for encrypting function pointers and decrypting them at runtime. Designed for runtime obfuscation and resistance to static analysis.

---

## Overview

`lazy-function-pointer-encryptor` wraps any function pointer in an encrypted container. The pointer is XOR-encrypted with a randomly generated key at construction time and is only decrypted immediately before a call is made. After the call completes the pointer is re-encrypted, so the plaintext address is never resident in memory for longer than necessary.

The decryption and dispatch is handled through a small x64 assembly stub allocated in executable memory. The stub XORs the encrypted address with the key at call time and jumps to the result, meaning the real function address is never written in plaintext to a fixed memory location.

---

## Requirements

- Windows (uses `VirtualAlloc` and `PAGE_EXECUTE_READWRITE`)
- 64-bit target architecture (x86-64)
- C++17 or later (`if constexpr` is required)

---

## Installation

The library is a single header file. Copy `EFP.hpp` into your project and include it.

```cpp
#include "EFP.hpp"
```

No build step, no dependencies beyond the Windows SDK and the C++ standard library.

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

int main() {
    // Wrap a function that returns int and takes two ints
    EncryptedFunction<int, int, int> encAdd(&Add);

    // Call it exactly like the original function
    int result = encAdd(3, 7);  // returns 10

    // Wrap a void function
    EncryptedFunction<void, const char*> encPrint(&PrintMessage);
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

1. **Construction** - The constructor stores the raw function pointer bytes, generates a random single-byte XOR key (1-255), and immediately XOR-encrypts the stored bytes. The plaintext pointer is discarded.

2. **Call** - When `operator()` is invoked, `decrypt()` XOR-decrypts the stored bytes back into a temporary `FuncType` value and calls `buildStub()`.

3. **Stub generation** - `buildStub()` allocates a small region of executable memory via `VirtualAlloc` and writes a minimal x64 sequence into it:
   ```
   mov rax, <encrypted_address>
   xor rax, <key>
   jmp rax
   ```
   The encrypted address and key are embedded directly in the instruction stream so the final target is only computed at the moment of execution.

4. **Dispatch** - The stub is called with the original arguments. For non-void return types the result is saved before continuing.

5. **Re-encryption** - After the call returns, `encrypt()` XORs the stored bytes again with the same key, leaving no plaintext pointer in memory.

---

## API reference

### `EncryptedFunction<Ret, Args...>`

**Constructor**

```cpp
EncryptedFunction(Ret(*func)(Args...))
```

Stores and immediately encrypts `func`. A new random key is generated for each instance.

**Call operator**

```cpp
Ret operator()(Args... args)
```

Decrypts the pointer, calls the function through the generated stub, re-encrypts, and returns the result. Transparent drop-in replacement for a direct function call.

**`void encrypt()`**

XOR-encrypts the stored pointer bytes using the instance key and sets the internal decrypted pointer to `nullptr`.

**`void decrypt()`**

XOR-decrypts the stored pointer bytes into a temporary value and builds the executable call stub.

---

## Notes

- XOR encryption is intended for obfuscation only. It is not a cryptographically secure scheme.
- The encryption key is stored alongside the encrypted data in the same object. A determined analyst with access to process memory can reconstruct the original pointer.
- **Memory leak**: Each call to `operator()` allocates a new executable stub via `VirtualAlloc`. This memory is never freed. In long-running processes or tight loops this will continuously consume virtual address space. Do not use in performance-critical or long-running code paths without accounting for this.
- **Security note**: Executable stubs are allocated with `PAGE_EXECUTE_READWRITE`, meaning the same memory region is simultaneously writable and executable. This is a known security risk and can make the process more susceptible to code-injection attacks. Be aware of this trade-off when deciding whether this library is appropriate for your threat model.
- The assembly stub is hardcoded for the x86-64 calling convention and will not work on 32-bit targets.

---

## License

See the repository for license information.
