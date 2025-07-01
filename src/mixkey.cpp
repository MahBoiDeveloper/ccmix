#include "mix_dexoder.h"
#include "../cryptopp/rsa.h"
#include "../cryptopp/blowfish.h"
#include "../cryptopp/base64.h"
#include "../cryptopp/integer.h"
#include "../cryptopp/hex.h"
#include "../cryptopp/osrng.h"
#include "../cryptopp/modes.h"
#include "../cryptopp/secblock.h"
#include <sstream>
#include <iostream>
#include <fstream>

extern const char* PUBKEY_DEFAULT_STR;
extern const char* PRVKEY_DEFAULT_STR;
const size_t ENC_KEY_LEN = 56;

using namespace CryptoPP;

const uint8_t INVERSE_ARRAY[] = { 0x8, 0xf, 0x6, 0x1, 0x5, 0x2, 0xb, 0xc, 0x3, 0x4, 0xd, 0xa, 0xe, 0x9, 0x0, 0x7 };
const uint8_t SHADOWS_ARRAY[] = { 0xe, 0x3, 0x5, 0x8, 0x9, 0x4, 0x2, 0xf, 0x0, 0xd, 0xb, 0x6, 0x7, 0xa, 0xc, 0x1 };

void BintToBfish(Integer& bint, uint8_t* key, int len = 56)
{
    const size_t buff_len = 56;
    uint8_t buffer[buff_len];
    bint.Encode(buffer, buff_len);
    
    int j = buff_len - 1;
    for(int i = 0; i < buff_len; i++)
    {
        key[i] = buffer[j];
        j--;
    }
}

int main(int argc, char* argv[])
{
    std::fstream ifh;
    std::fstream ofh;
    uint8_t keysource[80];
    uint8_t key[56];
    uint8_t nukey[56];
    uint8_t deckey[80];
    uint8_t buffer[80];
    uint8_t bfbuf[8];
    uint8_t keybuf[40];
    char* infile = argv[1];
    AutoSeededRandomPool prng;
    
    ifh.open(infile, std::ios_base::in|std::ios_base::binary);
    ifh.seekg(4);
    ifh.read((char*)keysource, 80);
    
    int j = 79;
    for(int i = 0; i < 80; i++) {
        buffer[j] = keysource[i];
        j--;
    }
    
    get_blowfish_key(keysource, key);
    std::string pubkey;
    std::string prvkey;
    Base64Decoder decode;
    decode.Put(reinterpret_cast<const byte*>(PUBKEY_DEFAULT_STR), ENC_KEY_LEN);
    pubkey.resize(decode.MaxRetrievable());
    decode.Get((byte*)pubkey.data(), pubkey.size());
    decode.Put(reinterpret_cast<const byte*>(PRVKEY_DEFAULT_STR), ENC_KEY_LEN);
    prvkey.resize(decode.MaxRetrievable());
    decode.Get((byte*)prvkey.data(), prvkey.size());
    
    pubkey.erase(0,2);
    prvkey.erase(0,2);
    
    Integer keyblkint(keysource, 80), bfshkeyint(key, 56);
    Integer keyblk1(buffer, 40), keyblk2(buffer + 40, 40);
    std::string keyblk((char*)keysource, 40), bfshkey((char*)key, 56);
    Integer n((byte*)pubkey.data(), pubkey.size()), e("0x10001"), d((byte*)prvkey.data(), prvkey.size());
    
    /*
    n.Encode(keybuf, 40);
    for(int i = 0; i < 40; i++) {
        printf("%02x", keybuf[i]);
    }
    std::wcout << "\n\n";
     
    d.Encode(keybuf, 40);
    for(int i = 0; i < 40; i++) {
        printf("%02x", keybuf[i]);
    }
    std::wcout << "\n\n";
    */
    
    //std::wcout << std::dec << n.BitCount() << "\n";
    //std::wcout << std::hex << n << "\n";
    //std::wcout << std::dec << e.BitCount() << "\n";
    //std::wcout << std::hex << e << "\n\n";
    //std::wcout << std::dec << d.BitCount() << "\n";
    //std::wcout << std::hex << d << "\n\n";
    
    std::cout << "XCC code blowfish key, encoded part 1 and 2:" << std::endl;
    std::cout << std::hex << bfshkeyint << std::endl;
    std::cout << std::hex << keyblk1 << std::endl;
    std::cout << std::hex << keyblk2 << std::endl << std::endl;
    
    AutoSeededRandomPool rng;
    
    RSA::PrivateKey rsaprvKey;
    rsaprvKey.Initialize(n, e, d);

    RSA::PublicKey rsapubKey;
    rsapubKey.Initialize(n, e);
    
    std::stringstream str;
    
    //std::wcout << "pre-enc cpp block\n" << std::hex << keyblk1 << "\n";
    Integer dec1 = rsaprvKey.ApplyFunction(keyblk1);
    //std::wcout << std::hex << dec1 << "\n";
    
    //std::wcout << "pre-enc cpp block\n" << std::hex << keyblk2 << "\n";
    Integer dec2 = rsaprvKey.ApplyFunction(keyblk2);
    //std::wcout << std::hex << dec2 << "\n\n";
    
    Integer blowfishkey = (dec1 << 312) + dec2;
    //std::wcout << std::dec << blowfishkey.BitCount() << "\n";
    //std::wcout << std::hex << blowfishkey << "\n";
    
    std::cout << "RSA blowfish key, decoded part1 and 2:" << std::endl;
    std::cout << std::hex << blowfishkey << std::endl;
    std::cout << std::hex << dec1 << std::endl;
    std::cout << std::hex << dec2 << std::endl << std::endl;
    
    BintToBfish(blowfishkey, nukey);
    //Integer testkey(nukey, 56);
    //std::wcout << std::hex << testkey << "\n";
    
    ECB_Mode< Blowfish >::Decryption bfdecrypt;
    bfdecrypt.SetKey(nukey, 56);
    ifh.read((char*)bfbuf, 8);
    bfdecrypt.ProcessString(bfbuf, 8);
    //std::wcout << "Decrypted num of files: " << std::dec << *(int16_t*)bfbuf << "\n";
}
