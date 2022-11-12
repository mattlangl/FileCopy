//
//        fileserver.cpp
//
//     Author: Matt Langley (mlangl02) and Caleb Pekowsky (cpekow01)
//

#include "c150nastyfile.h"
#include "c150nastydgmsocket.h"
#include "c150debug.h"
#include <fstream>
#include <cstdlib>
#include "filehelper.h"
#include <dirent.h>

using namespace C150NETWORK; // for all the comp150 utilities

unsigned char *checkHash(char *buffer, int startIndx, int numBytes, int nastiness);

const int networkArg = 1; // server name is 1st arg
const int fileArg = 2;    // nastiness name is 2nd arg
const int targetArg = 3;  // src name is 3rd arg

// Struct to store the current state of a file. Specifically, a buffer with it's currently copied-over
// contents, the total size of the file, and if it's done.
struct State
{
    char *buffer;
    unsigned int sz = 0;
    bool done = false;
    bool copied = false;
    string fname;
};

// Global data structures to hold data. a vector states, and a map to map a filename to it's corresponding vector index.
vector<State *> inProg;
unordered_map<std::string, int> fileNameToFileID;

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                           main program
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

int main(int argc, char *argv[])
{

    //
    // Variable declarations
    //
    ssize_t readlen;           // amount of data read from socket
    char incomingMessage[512]; // received message data
    int nastiness;             // how aggressively do we drop packets, etc?

    //
    // Check command line and parse arguments
    //

    GRADEME(argc, argv);

    if (argc != 4)
    {
        fprintf(stderr, "Correct syntxt is: %s <networknastiness> <filenastiness> <targetdir>\n", argv[0]);
        exit(1);
    }
    if (strspn(argv[1], "0123456789") != strlen(argv[1]) && strspn(argv[2], "0123456789") != strlen(argv[2]))
    {
        fprintf(stderr, "Nastiness %s is not numeric\n", argv[1]);
        fprintf(stderr, "Correct syntxt is: %s <nastiness_number>\n", argv[0]);
        exit(4);
    }
    nastiness = atoi(argv[1]); // convert command line string to integer

    //
    // Create socket, loop receiving and responding
    //
    try
    {
        //
        // Create the socket
        //
        C150DgmSocket *sock = new C150NastyDgmSocket(nastiness);

        string outputName = "";
        //
        // infinite loop processing messages
        //
        while (1)
        {
            readlen = sock->read(incomingMessage, 512);
            if (readlen == 0)
            {
                continue;
            }

            char w[512];

            // Every message's first character tells us what type of packet it is, so we switch on that.
            switch (incomingMessage[0])
            {

                /*
                 *  S : Refers to the start of transmissions of files.
                 *  this tells us we need to create new fileName : ID mapping and element in array
                 */

            case 's':
            {
                StartPacket response = *(reinterpret_cast<StartPacket *>(incomingMessage));
                // If we haven't seen this fileID, we add this filename and it's new ID to our data structures.
                if (fileNameToFileID.find(response.name) == fileNameToFileID.end())
                {
                    cout << "File: " << response.name << " starting to receive file" << endl;
                    *GRADING << "File: " << response.name << " starting to receive file" << endl;
                    int newIndex = inProg.size();
                    fileNameToFileID[response.name] = newIndex;
                    inProg.push_back(new State);
                    inProg[newIndex]->buffer = nullptr;
                    inProg[newIndex]->fname = response.name;
                }
                State *newState = inProg[fileNameToFileID[response.name]];

                if (newState->done)
                    continue;

                // checks to see if we need to update state size/buffer.
                if (newState->buffer != nullptr && newState->sz != response.fileSz)
                {
                    cout << response.fileSz << endl;
                    free(newState->buffer);
                }
                if (newState->buffer == nullptr)
                {
                    newState->sz = response.fileSz;
                    newState->buffer = (char *)malloc(newState->sz);
                }

                // Making and sending a response packet.
                StartResponsePacket pckt;
                pckt.cmd = 's';
                memcpy(pckt.name, response.name, (sizeof(response.name)));
                pckt.fileId = fileNameToFileID[response.name];
                pckt.fileSz = response.fileSz;
                memcpy(incomingMessage, &pckt, sizeof(pckt));
                sock->write(incomingMessage, sizeof(incomingMessage));
                break;
            }

                /*
                 *  I: In-transmission packets, which hold data from the file specified by via packetID.
                 */

            case 'i':
            {
                TransmissionPacket response = *(reinterpret_cast<TransmissionPacket *>(incomingMessage));
                // Copying first 12 bytes of response to the sendPacket, which is what we will send
                char sendPacket[sizeof(TransmissionResponsePacket)];
                memcpy(sendPacket, &response, sizeof(sendPacket));

                // getting the current file's state.
                State *currFile = inProg[response.fileId];

                // duplicate message handling
                if (currFile->done)
                    continue;

                // writing to current file's buffer
                unsigned int bytes = min(unsigned(SEND_SIZE), currFile->sz - (response.packetId * SEND_SIZE));
                for (unsigned int i = 0; i < bytes; i++)
                {
                    currFile->buffer[i + (response.packetId * SEND_SIZE)] = response.bytes[i];
                }

                break;
            }
                /*
                 *  E: end-to-end packets, which tell the server an end-to-end check on a series of packets has started.
                 *  contains the filename, the sequence of packets and the hash of that sequence.
                 */

            case 'e':
            {

                EndToEndPacket incoming = *(reinterpret_cast<EndToEndPacket *>(incomingMessage));
                // Getting corresponding state and how many bytes we're doing end-to-end check on.
                State *state = inProg[incoming.fileId];

                if (state->done)
                    continue;

                unsigned int bytes = min(unsigned(CHECK_SIZE * SEND_SIZE), state->sz - incoming.packetId * SEND_SIZE);

                // Building send packet
                EndToEndResponsePacket pckt;
                pckt.cmd = 'e';
                pckt.fileId = incoming.fileId;
                pckt.packetId = incoming.packetId;

                // Comparing hash values of the given bytes and the corresponding buffer's bytes.
                unsigned char *hash = checkHash(state->buffer, incoming.packetId, bytes, stoi(argv[fileArg]));
                memcpy(pckt.obuf, hash, sizeof(pckt.obuf));
                memcpy(w, &pckt, sizeof(pckt));

                sock->write(w, sizeof(EndToEndResponsePacket));
                break;
            }
                /*
                 *  F: end-to-end packets, which tell the server an end-to-end check on an entire file has started.
                 *  contains the file ID.
                 */

            case 'f':
            {
                EndToEndPacket incoming = *(reinterpret_cast<EndToEndPacket *>(incomingMessage));

                // Getting corresponding state
                State *state = inProg[incoming.fileId];
                string finishName = state->fname;

                // Creating temporary file to write buffer holding file data.
                finishName += ".tmp";
                string finishDir = makeFileName(argv[targetArg], finishName);
                unsigned char obuf[20];

                cout << "PERFORMING FINAL END TO END CHECK ON " << finishName << endl;
                *GRADING << "File: " << state->fname << " received, beginning end-to-end check" << endl;

                if (state->done)
                    continue;

                // While loop, writing to tmp file until the write is correct
                while (!state->copied)
                {
                    // Writing buffer to file
                    NASTYFILE outputFile(atoi(argv[fileArg]));
                    outputFile.fopen(finishDir.c_str(), "wb");

                    outputFile.fwrite(state->buffer, 1, state->sz);

                    if (outputFile.fclose() != 0)
                    {
                        break;
                        cerr << "Error closing output file " << finishName << " errno=" << strerror(errno) << endl;
                    }

                    // Getting hash of file.
                    string hash = hashFile(argv[targetArg], finishName, nastiness);
                    // unsigned char obuf[20];
                    SHA1((const unsigned char *)state->buffer,
                         state->sz, obuf);

                    // Comparing file hash and buffer hash.
                    if (hash == getHexRepresentation(obuf, 20))
                    {
                        // *GRADING << "File: " << response.name << " end-to-end check succeeded." << endl;
                        state->copied = true;
                    }
                }

                // Send response
                EndToEndResponsePacket pckt;
                pckt.cmd = 'f';
                pckt.fileId = incoming.fileId;
                pckt.packetId = 0;

                memcpy(pckt.obuf, obuf, sizeof(pckt.obuf));
                memcpy(w, &pckt, sizeof(pckt));

                sock->write(w, sizeof(EndToEndResponsePacket));
            }

                /*
                 *  C: confirm packets, which tell the server an end-to-end check has finished, and the result has
                 *  been acknowledged.
                 */

            case 'c':
            {
                // we just want to confirm we know the file did/didn't pass end-to-end check, so we
                // can just send back this message
                ConfirmPacket response = *(reinterpret_cast<ConfirmPacket *>(incomingMessage));
                int id = fileNameToFileID[response.name];
                string fname = makeFileName(argv[targetArg], response.name);
                string oldName = fname;
                oldName += ".tmp";
                // if the end-to-end check succeeded, we rename the file by removing the 'tmp'. otherwise, we set
                // the state of that file ID's copied to be false.
                if (!inProg[id]->done)
                {

                    if (response.success == true)
                    {
                        *GRADING << "File: " << inProg[id]->fname << " end-to-end check succeeded" << endl;
                        cout << "File: " << inProg[id]->fname << " transmission completed." << endl;
                        rename(oldName.c_str(), fname.c_str());
                        inProg[id]->done = true;
                        free(inProg[id]->buffer);
                    }
                    else
                    {
                        *GRADING << "File: " << inProg[id]->fname << " end-to-end check failed" << endl;
                        cout << "File: " << inProg[id]->fname << " end-to-end check failed, retrying." << endl;
                        inProg[id]->copied = false;
                    }
                    remove(oldName.c_str());
                }

                sock->write(incomingMessage, sizeof(ConfirmPacket));

                break;
            }
            default:
            {
                break;
            }
            }
        }
    }

    catch (C150NetworkException &e)
    {
        // Write to debug log
        c150debug->printf(C150ALWAYSLOG, "Caught C150NetworkException: %s\n",
                          e.formattedExplanation().c_str());
        // In case we're logging to a file, write to the console too
        cerr << argv[0] << ": caught C150NetworkException: " << e.formattedExplanation() << endl;
    }

    // This only executes if there was an error caught above
    return 4;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                           checkHash
//      this function computes the hash of a given buffer.
//
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

unsigned char *checkHash(char *buffer, int startIndx, int numBytes, int nastiness)
{

    unsigned char *obuf = (unsigned char *)malloc(20);
    SHA1((const unsigned char *)(buffer + (startIndx * SEND_SIZE)),
         numBytes, obuf);
    return obuf;
}
