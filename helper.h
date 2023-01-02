char encBuf[256] = {0}; // Let's make sure we have enough space for the encrypted string
char decBuf[256] = {0}; // Let's make sure we have enough space for the decrypted string

int16_t decryptECB(uint8_t* myBuf, uint8_t olen, uint8_t *pKey) {
  // Test the total len vs requirements:
  // AES: min 16 bytes
  // HMAC if needed: 28 bytes
  uint8_t reqLen = 16;
  if (olen < reqLen) return -1;
  uint8_t len;
  // or just copy over
  memcpy(encBuf, myBuf, olen);
  len = olen;
  struct AES_ctx ctx;
  AES_init_ctx(&ctx, pKey);
  uint8_t rounds = len / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    // void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
    AES_ECB_decrypt(&ctx, (uint8_t*)encBuf + steps);
    steps += 16;
    // decrypts in place, 16 bytes at a time
  } encBuf[steps] = 0;
  return len;
}

uint16_t encryptECB(uint8_t* myBuf, uint8_t *pKey) {
  // first ascertain length
  uint8_t len = strlen((char*)myBuf);
  uint16_t olen;
  struct AES_ctx ctx;
  olen = len;
  if (olen != 16) {
    if (olen % 16 > 0) {
      if (olen < 16) olen = 16;
      else olen += 16 - (olen % 16);
    }
  }
  memset((char*)encBuf, (olen - len), olen);
  memcpy((char*)encBuf, myBuf, len);
  encBuf[len] = 0;
  AES_init_ctx(&ctx, pKey);
  uint8_t rounds = olen / 16, steps = 0;
  for (uint8_t ix = 0; ix < rounds; ix++) {
    AES_ECB_encrypt(&ctx, (uint8_t*)(encBuf + steps));
    steps += 16;
    // encrypts in place, 16 bytes at a time
  }
  return olen;
}

uint16_t processCTR(uint8_t* myBuf, uint8_t *pKey, uint8_t *pIV) {
  // first ascertain length
  uint8_t len = strlen((char*)myBuf);
  uint16_t olen;
  struct AES_ctx ctx;
  olen = len;
  if (olen != 16) {
    if (olen % 16 > 0) {
      if (olen < 16) olen = 16;
      else olen += 16 - (olen % 16);
    }
  }
  memset((char*)encBuf, (olen - len), olen);
  memcpy((char*)encBuf, myBuf, len);
  //encBuf[len] = 0;
  AES_init_ctx_iv(&ctx, pKey, pIV);
  AES_CTR_xcrypt_buffer(&ctx, (uint8_t*)encBuf, olen);
  return olen;
}

void hexDump(unsigned char *buf, uint16_t len) {
  char alphabet[17] = "0123456789abcdef";
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
  Serial.print(F("   |.0 .1 .2 .3 .4 .5 .6 .7 .8 .9 .a .b .c .d .e .f | |      ASCII     |\n"));
  for (uint16_t i = 0; i < len; i += 16) {
    if (i % 128 == 0)
      Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
    char s[] = "|                                                | |                |\n";
    uint8_t ix = 1, iy = 52;
    for (uint8_t j = 0; j < 16; j++) {
      if (i + j < len) {
        uint8_t c = buf[i + j];
        s[ix++] = alphabet[(c >> 4) & 0x0F];
        s[ix++] = alphabet[c & 0x0F];
        ix++;
        if (c > 31 && c < 128) s[iy++] = c;
        else s[iy++] = '.';
      }
    }
    uint8_t index = i / 16;
    if (i < 256) Serial.write(' ');
    Serial.print(index, HEX); Serial.write('.');
    Serial.print(s);
  }
  Serial.print(F("   +------------------------------------------------+ +----------------+\n"));
}
