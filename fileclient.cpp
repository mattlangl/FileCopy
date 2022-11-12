//
//        fileclient.cpp
//
//     Author: Matt Langley (mlangl02) and Caleb Pekowsky (cpekow01)
//

#include "filehelper.h"
#include "c150nastyfile.h"
#include "c150nastydgmsocket.h"
#include "c150debug.h"
#include <fstream>
#include "c150grading.h"
#include <filesystem>
#include <openssl/sha.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring> // for errno string formatting
#include <cerrno>
#include <cstring> // for strerro
#include <fstream> // for input files
#include <vector>

using namespace std;         // for C++ std library
using namespace C150NETWORK; // for all the comp150 utilities

// forward declarations
void checkAndPrintMessage(ssize_t readlen, char *buf, ssize_t bufferlen);
void setUpDebugLogging(const char *logname, int argc, char *argv[]);
void runFileCopy(char *serverName, int netnast, int filenast, char *source);
void startMsg(C150NastyDgmSocket *sock, WriteHelper helper, char *fname, string dir);
Hash *transmitFile(C150NastyDgmSocket *sock, WriteHelper helper, char *buffer, size_t sourceSize, unsigned int fileId, char *fname);
unsigned int fileSize(string sourceDir, string fileName);
bool endToEndCheck(C150NastyDgmSocket *sock, WriteHelper helper, int fileId, Hash *h, char *fname);
void confirmMsg(C150NastyDgmSocket *sock, WriteHelper helper, char *fname, string dir, bool endToEnd);
size_t openFile(char *fname, char **buffer, string dir, int filenast);

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                    Command line arguments
//
// The following are used as subscripts to argv, the command line arguments
// If we want to change the command line syntax, doing this
// symbolically makes it a bit easier.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const int serverArg = 1;  // server name is 1st arg
const int networkArg = 2; // nastiness name is 2nd arg
const int fileArg = 3;
const int srcArg = 4; // src name is 3rd arg

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                           main program
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int main(int argc, char *argv[])
{
    //
    //  Set up debug message logging
    //
    GRADEME(argc, argv);
    // send all the file's hashes in the current directory
    if (argc != 5)
    {
        fprintf(stderr, "Correct syntxt is: %s <servername> <filenastiness> <srcdir>\n", argv[0]);
        exit(1);
    }
    runFileCopy(argv[serverArg], stoi(argv[networkArg]), stoi(argv[fileArg]), argv[srcArg]);

    return 0;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     runFileCopy
//
//        Copy every file in a directory to a server.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void runFileCopy(char *serverName, int netnast, int filenast, char *source)
{

    //
    //  Set up debug message logging
    //
    //
    //
    //        Send / receive / print
    //
    try
    {
        // Create the socket
        C150NastyDgmSocket *sock = new C150NastyDgmSocket(netnast);
        WriteHelper helper = WriteHelper();

        // Tell the DGMSocket which server to talk to
        sock->setServerName(serverName);

        // Turn on timeouts.
        sock->turnOnTimeouts(200);

        // Create value to hold string.
        string msg = "";

        // First check if the directory is valid.
        checkDirectory(source);

        // Open the directory and make sure it's not null.
        DIR *SRC = opendir(source);
        if (SRC == NULL)
        {
            fprintf(stderr, "Error opening source directory %s\n", source);
            exit(8);
        }

        unsigned int fileId = 0;

        // Looping throug ever file in the directory, transfering file and doing
        // end-to-end check on every file.
        struct dirent *dirEntry; // Directory entry for source file
        while ((dirEntry = readdir(SRC)) != NULL)
        {
            // skip the . and .. names
            if ((strcmp(dirEntry->d_name, ".") == 0) ||
                (strcmp(dirEntry->d_name, "..") == 0))
                continue; // never copy . or ..

            *GRADING << "File: " << dirEntry->d_name << " beginning transmission" << endl;

            startMsg(sock, helper, dirEntry->d_name, string(source));

            bool endCheck = false;

            // Malloc space for a char pointer that points to a buffer that holds file data.
            char **buffer = (char **)malloc(sizeof(char **));
            size_t sourceSize = openFile(dirEntry->d_name, buffer, string(source), filenast);

            int transmissionAttempt = 0;

            // Keep on sending file until end-to-end check succeeds.
            while (!endCheck)
            {
                transmissionAttempt++;

                // Sending the file data to the server
                Hash *hash = transmitFile(sock, helper, *buffer, sourceSize, fileId, dirEntry->d_name);
                *GRADING << "File: " << dirEntry->d_name << " transmission complete, waiting for end-to-end check, attempt " << transmissionAttempt << endl;

                // Doing end-to-end check
                endCheck = endToEndCheck(sock, helper, fileId, hash, dirEntry->d_name);
                confirmMsg(sock, helper, dirEntry->d_name, string(source), endCheck);
                if (!endCheck)
                {
                    *GRADING << "File: " << dirEntry->d_name << " end-to-end check failed, attempt " << transmissionAttempt << endl;
                }
                delete hash;
            }

            *GRADING << "File: " << dirEntry->d_name << " end-to-end check succeeded, attempt " << transmissionAttempt << endl;
            cout << "File: " << dirEntry->d_name << " transmission complete." << endl;

            fileId++;
            free(*buffer);
            free(buffer);
        }

        closedir(SRC);
        delete sock;
    }

    //  Handle networking errors -- for now, just print message and give up!
    //
    catch (C150NetworkException &e)
    {
        // Write to debug log
        // In case we're logging to a file, write to the console too
        cerr << "./fileserver : caught C150NetworkException: " << e.formattedExplanation()
             << endl;
    }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     transmitFile
//
//      attempts to send a file fname to the chosen socket
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

Hash *transmitFile(C150NastyDgmSocket *sock, WriteHelper helper, char *buffer, size_t sourceSize, unsigned int fileId, char *fname)
{

    // Get hash of buffer storing file data, store in obuf
    unsigned char obuf[20];
    SHA1((const unsigned char *)buffer,
         sourceSize, obuf);

    // Calculate number of packets to send..
    unsigned int ttlPackets = sourceSize / SEND_SIZE;
    if (sourceSize % SEND_SIZE != 0)
        ttlPackets++;
    TransmissionPacket send;
    cout << "BEGINNING TRANSMISSION \n";

    // Looping through every single "block" of bytes that can fit in a packet.
    int attempts = 1;
    for (unsigned int i = 0; i < ttlPackets; i++)
    {
        // Logic to handle if we want to send last x bytes, and x is less than 500.
        int num = min(SEND_SIZE, int(sourceSize - (i * SEND_SIZE)));
        // Copy this x bytes to a packets contents, and send it
        char *val = buffer + (i * SEND_SIZE);
        memcpy(&(send.bytes), val, num);
        send.fileId = fileId;
        send.packetId = i;
        helper.writeMsg(sock, send, 50);
        // This tells us if we need to do an end-to-end check on this sequence of bytes.
        if (i % CHECK_SIZE == CHECK_SIZE - 1 || i == ttlPackets - 1)
        {

            // Getting sequence of bytes we want to end-to-end check, and the hash of this sequence.
            int num = i - (i % CHECK_SIZE);
            unsigned int bytes = min(size_t(CHECK_SIZE * SEND_SIZE), sourceSize - num * SEND_SIZE);
            unsigned char obuf[20];
            SHA1((const unsigned char *)(buffer + num * SEND_SIZE),
                 bytes, obuf);

            // Creating send packet.
            EndToEndPacket pckt;
            pckt.cmd = 'e';
            pckt.fileId = fileId;
            pckt.packetId = num;

            // Getting hash of server's sequence of bytes
            EndToEndResponsePacket response = helper.writeMsg(sock, pckt, 10);

            // Checking if hashes are identical
            bool passed = true;
            for (long unsigned int j = 0; j < 20; j++)
            {
                if (obuf[j] != response.obuf[j])
                {
                    passed = false;
                    break;
                }
            }
            if (passed)
            {
                attempts = 0;
                // If end-to-end check fails on this packet, we go back to beginning of this "block"
            }
            else
            {
                *GRADING << "File: " << fname << " packets number: " << i - i % CHECK_SIZE
                         << " through: " << i << " transmission failed on attempt " << attempts << ".  Retrying transmsision." << endl;
                attempts++;
                i -= i % CHECK_SIZE;
            }
        }
    }
    cout << "TRANSMISSION COMPLETED\n";
    return newHash(obuf);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     startMsg
//
//    Send packet telling server we are starting to send a file
//.   by sending packet of type "s" filename fileSize.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void startMsg(C150NastyDgmSocket *sock, WriteHelper helper, char *fname, string dir)
{

    cout << "STARTING FILE TRANSFER ON " << fname << endl;
    // We send the length of the file so the server knows when to stop accepting packets.
    StartPacket pckt;
    pckt.cmd = 's';
    pckt.fileSz = fileSize(dir, fname);
    strcpy(pckt.name, fname);
    helper.writeMsg(sock, pckt, 10);
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     endToEndCheck
//
//    returns boolean saying if the file corresonding to fileId is identical
//    on client and server.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

bool endToEndCheck(C150NastyDgmSocket *sock, WriteHelper helper, int fileId, Hash *hash, char *fname)
{

    EndToEndPacket pckt;
    pckt.cmd = 'f';
    pckt.fileId = fileId;
    pckt.packetId = 0;

    // Send packet to server to get server's hash of file corresponding to file ID
    EndToEndResponsePacket response = helper.writeMsg(sock, pckt, 10);

    // checking if the given hash and server's hash are equal.
    bool endToEnd = true;
    for (int i = 0; i < 20 && endToEnd; i++)
    {
        if (hash->obuf[i] != response.obuf[i])
            endToEnd = false;
    }

    return endToEnd;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     confirmMsg
//
//    Send packet telling server we acknowledge the result of an end-to-end check
//.   by sending packet of type "c" filename.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void confirmMsg(C150NastyDgmSocket *sock, WriteHelper helper, char *fname, string dir, bool endToEnd)
{
    // Creating and sending packet of type c status
    ConfirmPacket endPacket;
    endPacket.cmd = 'c';
    endPacket.success = endToEnd;
    strcpy(endPacket.name, fname);
    // response should be of type  e filenamename success/failure
    helper.writeMsg(sock, endPacket, 10);
}

void checkAndPrintMessage(ssize_t readlen, char *msg, ssize_t bufferlen)
{
    //
    // Except in case of timeouts, we're not expecting
    // a zero length read
    //
    if (readlen == 0)
    {
        throw C150NetworkException("Unexpected zero length read in client");
    }

    // DEFENSIVE PROGRAMMING: we aren't even trying to read this much
    // We're just being extra careful to check this
    if (readlen > (int)(bufferlen))
    {
        throw C150NetworkException("Unexpected over length read in client");
    }

    //
    // Make sure server followed the rules and
    // sent a null-terminated string (well, we could
    // check that it's all legal characters, but
    // at least we look for the null)
    //
    if (msg[readlen - 1] != '\0')
    {
        throw C150NetworkException("Client received message that was not null terminated");
    };

    //
    // Use a routine provided in c150utility.cpp to change any control
    // or non-printing characters to "." (this is just defensive programming:
    // if the server maliciously or inadvertently sent us junk characters, then we
    // won't send them to our terminal -- some
    // control characters can do nasty things!)
    //
    // Note: cleanString wants a C++ string, not a char*, so we make a temporary one
    // here. Not super-fast, but this is just a demo program.
    string s(msg);
    cleanString(s);

    // Echo the response on the console
    printf("Response received is \"%s\"\n", s.c_str());
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     fileSize
//
//              gets the size of a file.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

unsigned int fileSize(string sourceDir, string fileName)
{

    //
    //  Misc variables, mostly for return codes
    //
    struct stat statbuf;
    size_t sourceSize;

    //
    // Read whole input file
    //
    string sourceName = makeFileName(sourceDir, fileName);
    if (lstat(sourceName.c_str(), &statbuf) != 0)
    {
        string err = "File not found";
        throw C150Exception(err + sourceName);
    }

    //
    // Make an input buffer large enough for
    // the whole file
    //
    sourceSize = statbuf.st_size;

    return sourceSize;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     openFile
//
//             opens a file, and reads it into a buffer, 
//              handling file nastiness.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

size_t openFile(char *fname, char **buffer, string dir, int filenast)
{

    string sourceName = makeFileName(dir, fname);
    struct stat statbuf;

    // This check should never fail.
    if (lstat(sourceName.c_str(), &statbuf) != 0)
    {
        fprintf(stderr, "copyFile: Error stating supplied source file %s\n", sourceName.c_str());
        exit(20);
    }
    // open file
    size_t sourceSize = statbuf.st_size;
    *buffer = (char *)malloc(sourceSize * 4);
    // edge case: if sourceSize is 0, we just return 0.
    if (sourceSize == 0)
        return 0;
    // size_t len;

    /*
    check if the buffer is equal to the file, and keep on opening it until it is.
    to do this, after we read the buffer into the file, we do SHA-1 hash comparison between the file and the buffer.
    if this succeeds 5x, we continue.
    NEEDS WORK: if it copies incorrectly the same way 5x, it'll think it did it correctly.
    */

    unsigned long COPY_BLOCK_SIZE = sourceSize / 2;

    // crucial if file size is 1 to ensure not trying to copy blocks of size 0
    if (sourceSize % 2 == 1)
    {
        COPY_BLOCK_SIZE++;
    }

    bool removeByte = false;

    unsigned long ttlBlocks = sourceSize / COPY_BLOCK_SIZE;
    if (sourceSize % COPY_BLOCK_SIZE != 0)
        ttlBlocks++;


     // we later add and then remove a byte to the file if the file size is 1 as the way we ensure a file is opened correctly
     // is by reading the file in different byte sizes.

    if (sourceSize == 1)
    {
        removeByte = true;
        ttlBlocks++;
        sourceSize++;
    }

    NASTYFILE inputFile(filenast);
    inputFile.fopen(sourceName.c_str(), "r+");

    unsigned char obuf[20];
    unsigned char fbuf[20];
    unsigned char tbuf[20];

    bool copied = false;

    while (!copied)
    {
        // add a byte to the file if we need to
        if (removeByte)
        {
            char c = 'A';
            inputFile.fseek(1, SEEK_SET);
            inputFile.fwrite(&c, 1, 1);
        }



        // read the file again in blocks half the size of the file.
        // we found that the errors produced were different depending on what size blocks
        // we read the file in, so by reading the whole file at once and the whole file in two parts,
        // the errors produced when reading the file differ, so if we read a file in different block sizes
        // and get the same result, the file must have been read correctly.

        for (unsigned long i = 0; i < ttlBlocks; i++)
        {
            // bytes to copy
            int num = min(COPY_BLOCK_SIZE, sourceSize - (i * COPY_BLOCK_SIZE));

            char *copyBuffer = (char *)malloc(num);


            //copy bytes
            inputFile.fseek(i * COPY_BLOCK_SIZE, SEEK_SET);
            inputFile.fread(copyBuffer, 1, num);


            // hash bytes
            SHA1((const unsigned char *)copyBuffer,
                 num, obuf);

            int inARow = 0;


            // ensure we get the same value 5 times in a row (extra check, does nothing at high nastiness)
            while (inARow < 5)
            {
                inputFile.fseek(i * COPY_BLOCK_SIZE, 0);
                SHA1((const unsigned char *)copyBuffer,
                     num, tbuf);

                for (int j = 0; j < 20; j++)
                {
                    if (tbuf[j] != obuf[j])
                    {
                        inARow = -1;
                        break;
                    }
                }

                inARow++;
                memcpy(obuf, tbuf, 20);
            }

            memcpy((*buffer) + i * COPY_BLOCK_SIZE, copyBuffer, num);
            free(copyBuffer);
        }


        // hash the copied file
        SHA1((const unsigned char *)(*buffer),
             sourceSize, obuf);

        int attempts = 0;

        // try 
        while (!copied && attempts < 5)
        {
            copied = true;

            // read the whole file to the buffer
            inputFile.fseek(0, 0);
            inputFile.fread(*buffer, 1, sourceSize);

            // hash the whole file
            SHA1((const unsigned char *)(*buffer),
                sourceSize, fbuf);


            // if ever the same, it went through
            for (int i = 0; i < 20 && copied; i++)
            {
                if (obuf[i] != fbuf[i])
                {
                    copied = false;
                }
            }

            attempts++;
        }
    }

    if (inputFile.fclose() != 0)
    {
        cerr << "Error closing output file " << sourceName << " errno=" << strerror(errno) << endl;
    }

    // if the file size was 1, remove the extra byte from the original file before moving on.
    while (removeByte)
    {
        char c;
        inputFile.fopen(sourceName.c_str(), "w+");
        inputFile.fwrite((*buffer), 1, 1);
        inputFile.fseek(0, 0);
        inputFile.fread(&c, 1, 1);

        if (inputFile.fclose() != 0)
        {
            cerr << "Error closing output file " << sourceName << " errno=" << strerror(errno) << endl;
        }

        if (c == (*buffer)[0])
            continue;

        removeByte = false;
        sourceSize--;
    }

    return sourceSize;
}
