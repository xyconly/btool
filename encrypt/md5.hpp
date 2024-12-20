/************************************************************************     
如需要支持中文,需要外界转换为Unicode;

std::string toUnicode(const char* message) {
    int len = MultiByteToWideChar(CP_ACP, 0, message, -1, NULL, 0);
    wchar_t* wstr = new wchar_t[len + 1];
    memset(wstr, 0, len + 1);
    MultiByteToWideChar(CP_ACP, 0, message, -1, wstr, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, NULL, NULL);
    if (wstr) delete[] wstr;
    string rslt = str;
    if (str) delete[] str;
    return rslt;
}
************************************************************************/

#pragma once

#include <openssl/md5.h>
#include "encrypt_base.hpp"
// #include <codecvt>
// #include <string.h>

namespace BTool
{
//     typedef unsigned char byte;
//     typedef unsigned long ulong;

//         //步函数  
// #define MDF(x, y, z) (((x) & (y)) | ((~x) & (z)))  
// #define MDG(x, y, z) (((x) & (z)) | ((y) & (~z)))  
// #define MDH(x, y, z) ((x) ^ (y) ^ (z))  
// #define MDI(x, y, z) ((y) ^ ((x) | (~z)))  
//         //循环左移   
// #define ROL(x, n) (((x) << (n)) | ((x) >> (32-(n))))  
//         //  四轮操作  
// #define MDFF(a, b, c, d, mj, s, ti) { (a) += MDF ((b), (c), (d)) + (mj) + ti; (a) = ROL ((a), (s)); (a) += (b);}  
// #define MDGG(a, b, c, d, mj, s, ti) { (a) += MDG ((b), (c), (d)) + (mj) + ti; (a) = ROL ((a), (s)); (a) += (b);}  
// #define MDHH(a, b, c, d, mj, s, ti) { (a) += MDH ((b), (c), (d)) + (mj) + ti; (a) = ROL ((a), (s)); (a) += (b);}  
// #define MDII(a, b, c, d, mj, s, ti) { (a) += MDI ((b), (c), (d)) + (mj) + ti; (a) = ROL ((a), (s)); (a) += (b);}  

//         class MD5
//         {
//         public:
//             MD5(const std::string &str_unicode)
//             {
//                 end_flag = false;
//                 count[0] = count[1] = 0; // 重置bits个数  
//                                          // 初始化链接变量（高位->低位）   
//                                          /*
//                                          * 错误的赋值，注意高低位
//                                          reg[0] = 0x01234567;
//                                          reg[1] = 0x89abcdef;
//                                          reg[2] = 0xfedcba98;
//                                          reg[3] = 0x76543210;
//                                          */
//                 reg[0] = 0x67452301;
//                 reg[1] = 0xefcdab89;
//                 reg[2] = 0x98badcfe;
//                 reg[3] = 0x10325476;
//                 generate((byte*)str_unicode.c_str(), str_unicode.length());
//             }

//             // 返回字符串
//             std::string outstr(int length = 32)
//             {
//                 const byte *input = getDigest();
//                 std::string str;
//                 //str.reserve(length << 1);  
//                 for (int i = 0; i < 16; i++) {
//                     int t = input[i];
//                     int a = t / 16;
//                     int b = t % 16;
//                     str.append(1, hex[a]);
//                     str.append(1, hex[b]);
//                 }
//                 if (16 == length) {
//                     str = str.substr(9 - 1, 16);
//                 }
//                 return str;
//             }

//         private:
//             void generate(const byte* input, size_t length)
//             {
//                 ulong i, index, partLen;
//                 end_flag = false;
//                 index = (ulong)((count[0] >> 3) & 0x3f);
//                 if ((count[0] += ((ulong)length << 3)) < ((ulong)length << 3))
//                     count[1]++;
//                 count[1] += ((ulong)length >> 29);
//                 partLen = 64 - index;
//                 if (length >= partLen) {
//                     memcpy(&buffer[index], input, partLen);
//                     change(buffer); //分组处理，四轮操作   
//                     for (i = partLen; i + 63 < length; i += 64)
//                         change(&input[i]);
//                     index = 0;
//                 }
//                 else {
//                     i = 0;
//                 }
//                 // 内存拷贝   
//                 memcpy(&buffer[index], &input[i], length - i);

//             }

//             // 四轮变换操作
//             void change(const byte block[64])
//             {
//                 ulong a = reg[0],
//                     b = reg[1],
//                     c = reg[2],
//                     d = reg[3],
//                     m[16];

//                 decode(block, m, 64);   

//                 MDFF(a, b, c, d, m[0], 7, 0xd76aa478);
//                 MDFF(d, a, b, c, m[1], 12, 0xe8c7b756);
//                 MDFF(c, d, a, b, m[2], 17, 0x242070db);
//                 MDFF(b, c, d, a, m[3], 22, 0xc1bdceee);
//                 MDFF(a, b, c, d, m[4], 7, 0xf57c0faf);
//                 MDFF(d, a, b, c, m[5], 12, 0x4787c62a);
//                 MDFF(c, d, a, b, m[6], 17, 0xa8304613);
//                 MDFF(b, c, d, a, m[7], 22, 0xfd469501);
//                 MDFF(a, b, c, d, m[8], 7, 0x698098d8);
//                 MDFF(d, a, b, c, m[9], 12, 0x8b44f7af);
//                 MDFF(c, d, a, b, m[10], 17, 0xffff5bb1);
//                 MDFF(b, c, d, a, m[11], 22, 0x895cd7be);
//                 MDFF(a, b, c, d, m[12], 7, 0x6b901122);
//                 MDFF(d, a, b, c, m[13], 12, 0xfd987193);
//                 MDFF(c, d, a, b, m[14], 17, 0xa679438e);
//                 MDFF(b, c, d, a, m[15], 22, 0x49b40821);

//                 MDGG(a, b, c, d, m[1], 5, 0xf61e2562);
//                 MDGG(d, a, b, c, m[6], 9, 0xc040b340);
//                 MDGG(c, d, a, b, m[11], 14, 0x265e5a51);
//                 MDGG(b, c, d, a, m[0], 20, 0xe9b6c7aa);
//                 MDGG(a, b, c, d, m[5], 5, 0xd62f105d);
//                 MDGG(d, a, b, c, m[10], 9, 0x2441453);
//                 MDGG(c, d, a, b, m[15], 14, 0xd8a1e681);
//                 MDGG(b, c, d, a, m[4], 20, 0xe7d3fbc8);
//                 MDGG(a, b, c, d, m[9], 5, 0x21e1cde6);
//                 MDGG(d, a, b, c, m[14], 9, 0xc33707d6);
//                 MDGG(c, d, a, b, m[3], 14, 0xf4d50d87);
//                 MDGG(b, c, d, a, m[8], 20, 0x455a14ed);
//                 MDGG(a, b, c, d, m[13], 5, 0xa9e3e905);
//                 MDGG(d, a, b, c, m[2], 9, 0xfcefa3f8);
//                 MDGG(c, d, a, b, m[7], 14, 0x676f02d9);
//                 MDGG(b, c, d, a, m[12], 20, 0x8d2a4c8a);

//                 MDHH(a, b, c, d, m[5], 4, 0xfffa3942);
//                 MDHH(d, a, b, c, m[8], 11, 0x8771f681);
//                 MDHH(c, d, a, b, m[11], 16, 0x6d9d6122);
//                 MDHH(b, c, d, a, m[14], 23, 0xfde5380c);
//                 MDHH(a, b, c, d, m[1], 4, 0xa4beea44);
//                 MDHH(d, a, b, c, m[4], 11, 0x4bdecfa9);
//                 MDHH(c, d, a, b, m[7], 16, 0xf6bb4b60);
//                 MDHH(b, c, d, a, m[10], 23, 0xbebfbc70);
//                 MDHH(a, b, c, d, m[13], 4, 0x289b7ec6);
//                 MDHH(d, a, b, c, m[0], 11, 0xeaa127fa);
//                 MDHH(c, d, a, b, m[3], 16, 0xd4ef3085);
//                 MDHH(b, c, d, a, m[6], 23, 0x4881d05);
//                 MDHH(a, b, c, d, m[9], 4, 0xd9d4d039);
//                 MDHH(d, a, b, c, m[12], 11, 0xe6db99e5);
//                 MDHH(c, d, a, b, m[15], 16, 0x1fa27cf8);
//                 MDHH(b, c, d, a, m[2], 23, 0xc4ac5665);

//                 MDII(a, b, c, d, m[0], 6, 0xf4292244);
//                 MDII(d, a, b, c, m[7], 10, 0x432aff97);
//                 MDII(c, d, a, b, m[14], 15, 0xab9423a7);
//                 MDII(b, c, d, a, m[5], 21, 0xfc93a039);
//                 MDII(a, b, c, d, m[12], 6, 0x655b59c3);
//                 MDII(d, a, b, c, m[3], 10, 0x8f0ccc92);
//                 MDII(c, d, a, b, m[10], 15, 0xffeff47d);
//                 MDII(b, c, d, a, m[1], 21, 0x85845dd1);
//                 MDII(a, b, c, d, m[8], 6, 0x6fa87e4f);
//                 MDII(d, a, b, c, m[15], 10, 0xfe2ce6e0);
//                 MDII(c, d, a, b, m[6], 15, 0xa3014314);
//                 MDII(b, c, d, a, m[13], 21, 0x4e0811a1);
//                 MDII(a, b, c, d, m[4], 6, 0xf7537e82);
//                 MDII(d, a, b, c, m[11], 10, 0xbd3af235);
//                 MDII(c, d, a, b, m[2], 15, 0x2ad7d2bb);
//                 MDII(b, c, d, a, m[9], 21, 0xeb86d391);

//                 reg[0] += a;
//                 reg[1] += b;
//                 reg[2] += c;
//                 reg[3] += d;
//             }

//             // 编码,ulong->byte
//             void encode(const ulong *input, byte* output, int length)
//             {
//                 for (int i = 0, j = 0; j < length; i++, j += 4)
//                 {
//                     output[j] = (byte)(input[i] & 0xff);
//                     output[j + 1] = (byte)((input[i] >> 8) & 0xff);
//                     output[j + 2] = (byte)((input[i] >> 16) & 0xff);
//                     output[j + 3] = (byte)((input[i] >> 24) & 0xff);
//                 }
//             }

//             // 解码,byte->ulong
//             void decode(const byte *input, ulong *output, int length)
//             {
//                 for (int i = 0, j = 0; j < length; i++, j += 4)
//                 {
//                     output[i] = ((ulong)input[j]) | (((ulong)input[j + 1]) << 8) | (((ulong)input[j + 2]) << 16) | (((ulong)input[j + 3]) << 24);
//                 }
//             }

//             // 获取生成的摘要
//             const byte* getDigest()
//             {
//                 if (!end_flag) {
//                     end_flag = true;
//                     byte bits[8];
//                     ulong _reg[4];      // 旧reg   
//                     ulong _count[2];    // 旧count  
//                     ulong index, padLen;
//                     memcpy(_reg, reg, 16);      // 复制内存，将_reg内存地址的起始位置开始拷贝16字节到reg起始位置中  
//                     memcpy(_count, count, 8);   // 将原始消息长度以64比特（8字节）复制到count后   
//                     encode(count, bits, 8);
//                     /* Pad out to 56 mod 64. */
//                     index = (ulong)((count[0] >> 3) & 0x3f);
//                     padLen = (index < 56) ? (56 - index) : (120 - index);
//                     generate(padding, padLen);
//                     generate(bits, 8);
//                     encode(reg, digest, 16);
//                     /* 重新存储reg和count */
//                     memcpy(reg, _reg, 16);
//                     memcpy(count, _count, 8);
//                 }
//                 return digest;
//             }

//             ulong reg[4];       // ABCD   
//             ulong count[2];     // 长度扩充   
//             byte buffer[64];   // 输入buffer   
//             byte digest[16];   // 生成的摘要   
//             bool end_flag;      // 结束标志   
//             const byte padding[64] = { 0x80 };    // 初始化附加填充 1000 0000， 设置最高位为1
//             const char hex[16] = { '0', '1', '2', '3','4', '5', '6', '7','8', '9', 'a', 'b','c', 'd', 'e', 'f' };
//         };

    class MD5 : public EncryptBase {
    public:
        static std::string EncryptToHex(const char* data, size_t data_len, bool upper = true) {
            unsigned char digest[MD5_DIGEST_LENGTH];
            MD5_CTX ctx;
            MD5_Init(&ctx);
            MD5_Update(&ctx, data, data_len);
            MD5_Final(digest, &ctx);

            std::string signature(MD5_DIGEST_LENGTH * 2, '\0');
            HexToString((const char *)digest, MD5_DIGEST_LENGTH, (unsigned char*)signature.data(), upper);
            return signature;
        }

    };

}