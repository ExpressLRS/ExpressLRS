# hmac_sha256
*Minimal HMAC-SHA256 implementation in C / C++*

This repository provides minimal HMAC-Sha256 code you can copy into your own projects.
The `hmac_sha256` function looks like this:
```cpp
size_t // Returns the number of bytes written to `out`
hmac_sha256(
    // [in]: The key and its length.
    //      Should be at least 32 bytes long for optimal security.
    const void* key, const size_t keylen,

    // [in]: The data to hash alongside the key.
    const void* data, const size_t datalen,

    // [out]: The output hash.
    //      Should be 32 bytes long. If it's less than 32 bytes,
    //      the resulting hash will be truncated to the specified length.
    void* out, const size_t outlen
);
```

## Contributing
All contributions are welcome, feature requests, or issues.
I aim to tailor this code not only for myself, but for other's use cases too.

## Usage Example (C++)
https://github.com/h5p9sl/hmac_sha256/blob/79a57d2a85aaab32449e5179a4f08f37e38cdee5/examples/simple_example.cpp#L13-L26

```cpp
    const std::string str_data = "Hello World!";
    const std::string str_key = "super-secret-key";

    // Allocate memory for the HMAC
    std::vector<uint8_t> out(SHA256_HASH_SIZE);

    // Call hmac-sha256 function
    hmac_sha256(
        str_key.data(),  str_key.size(),
        str_data.data(), str_data.size(),
        out.data(),  out.size()
    );
```
## Sha256 Implementation
Big thank you to [WjCryptLib](https://github.com/WaterJuice/WjCryptLib) for providing the Sha256 implementation of which this project is based off.
If you need more public domain cryptographic functions in C (sha, aes, md5), check them out.
