//
//        filehelper.h
//
//     Author: Matt Langley (mlangl02) and Caleb Pekowsky (cpekow01)
//

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <openssl/sha.h>
#include <vector>
#include "c150nastydgmsocket.h"

using namespace std;
using namespace C150NETWORK; // for all the comp150 utilities

string getHexRepresentation(const unsigned char *bytes, size_t len);
void checkDirectory(char *dirname);
string hashFile(string sourceDir, string fileName, int nastiness);
string makeFileName(string dir, string name);

struct StartPacket
{
    char cmd;
    char name[255];
    unsigned int fileSz;
};

struct StartResponsePacket
{
    char cmd;
    char name[255];
    unsigned int fileId;
    unsigned int fileSz;
};

struct EndToEndPacket
{
    char cmd;
    unsigned int fileId;
    unsigned int packetId;
};

struct EndToEndFinalPacket
{
    char cmd;
    unsigned int fileId;
    unsigned int packetId;
};

struct EndToEndResponsePacket
{
    char cmd;
    unsigned int fileId;
    unsigned int packetId;
    unsigned char obuf[20];
};

struct ConfirmPacket
{
    char cmd;
    char name[255];
    bool success;
};

struct TransmissionPacket
{
    char cmd;
    unsigned int fileId;
    unsigned int packetId;
    char bytes[500];
    TransmissionPacket() : cmd('i'), fileId(0), packetId(0) {}
};

struct TransmissionResponsePacket
{
    char cmd;
    unsigned int fileId;
    unsigned int packetId;
    TransmissionResponsePacket() : cmd('i'), fileId(0), packetId(0) {}
};

struct Hash
{
    unsigned char obuf[20];
};

Hash *newHash(unsigned char obuf[20]);

class WriteHelper
{
private:
    char w[512];

public:
    StartPacket writeMsg(C150NastyDgmSocket *sock, StartPacket msg, int attempts);
    EndToEndResponsePacket writeMsg(C150NastyDgmSocket *sock, EndToEndPacket msg, int attempts);
    void writeMsg(C150NastyDgmSocket *sock, TransmissionPacket msg, int attempts);
    ConfirmPacket writeMsg(C150NastyDgmSocket *sock, ConfirmPacket msg, int attempts);
};

const int SEND_SIZE = 500;

const int CHECK_SIZE = 250;
