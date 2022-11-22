#ifndef DATA_ENCRYPTION_HEADER
#define DATA_ENCRYPTION_HEADER

#include "string"

/** NOTES
 * I'm afraid more complicated encryption will take MCU's resources.
 * So the solution are just plain and simple encryption using XOR operations
 * and using key created by randoms supplied by input peripherals. (user
 * clicking and suchs)
 */


class data_encryption{
  private:
    std::string _key;
  
  public:
    data_encryption();

    void useKey(std::string key);

    // note that in order to reduce memory used, the buffer will be overwritten
    // if the key isn't initialized, then no need to encrypt or decrypt
    void encryptOrDecryptData(char *buffer, size_t bufferlen);
    void encryptOrDecryptData(char *buffer, size_t bufferlen, size_t idx_offset);
};


#endif