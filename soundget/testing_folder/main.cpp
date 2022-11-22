#include "ByteIterator.hpp"
#include "data_encryption.hpp"
#include "stdio.h"

int main(){
  data_encryption _encrypt; _encrypt.useKey("52r87dur0uy7y");
  char data[sizeof(int)*3];
  ByteIteratorR_Encryption _bire(data, sizeof(int)*3, &_encrypt);
  _bire.setVar(3215445);
  _bire.setVar(12342152);
  _bire.setVar(3653345);

  printf("encrypted data: %s\n", data);

  ByteIterator *_bie = new ByteIterator(data, sizeof(int)*3);
  _bie = new ByteIterator_Encryption(data, sizeof(int)*3, &_encrypt);
  int a = 0; _bie->getVar(a);
  int b = 0; _bie->getVar(b);
  int c = 0; _bie->getVar(c);

  printf("a%d b%d c%d\n", a, b, c);
}