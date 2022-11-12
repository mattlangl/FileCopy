//
//        filehelper.cpp
//
//     Author: Matt Langley (mlangl02) and Caleb Pekowsky (cpekow01)
//

#include "c150nastyfile.h" // for c150nastyfile & framework
#include "c150grading.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring> // for errno string formatting
#include <cerrno>
#include <cstring>  // for strerro
#include <iostream> // for cout
#include <fstream>
#include <iomanip>
#include "filehelper.h"

using namespace C150NETWORK; // for all the comp150 utilities

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     getHexRepresentation
//
//        convert SHA-1 output to hex string representation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

string getHexRepresentation(const unsigned char *bytes, size_t len)
{
  ostringstream os;
  os.fill('0');
  os << std::hex;
  for (const unsigned char *ptr = bytes; ptr < bytes + len; ++ptr)
  {
    os << setw(2) << (unsigned int)*ptr;
  }
  return os.str();
}

void checkDirectory(char *dirname)
{
  struct stat statbuf;
  if (lstat(dirname, &statbuf) != 0)
  {
    fprintf(stderr, "Error stating supplied source directory %s\n", dirname);
    exit(8);
  }

  if (!S_ISDIR(statbuf.st_mode))
  {
    fprintf(stderr, "File %s exists but is not a directory\n", dirname);
    exit(8);
  }
}

string makeFileName(string dir, string name)
{
  stringstream ss;

  ss << dir;
  // make sure dir name ends in /
  if (dir.substr(dir.length() - 1, 1) != "/")
    ss << '/';
  ss << name;      // append file name to dir
  return ss.str(); // return dir/name
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     hashFile
//
//        creates a hash of a file.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

string hashFile(string sourceDir, string fileName, int nastiness)
{

  //
  //  Misc variables, mostly for return codes
  //
  void *fopenretval;
  size_t len;
  char *buffer;
  string errorString;
  struct stat statbuf;
  size_t sourceSize;

  try
  {

    //
    // Read whole input file
    //

    string sourceName = makeFileName(sourceDir, fileName);
    if (lstat(sourceName.c_str(), &statbuf) != 0)
    {
      string err = "File not found ";
      throw C150Exception(err + sourceName);
    }

    //
    // Make an input buffer large enough for
    // the whole file
    //
    sourceSize = statbuf.st_size;
    buffer = (char *)malloc(sourceSize);

    //
    // Define the wrapped file descriptors
    //
    // All the operations on outputFile are the same
    // ones you get documented by doing "man 3 fread", etc.
    // except that the file descriptor arguments must
    // be left off.
    //
    // Note: the NASTYFILE type is meant to be similar
    //       to the Unix FILE type
    //
    NASTYFILE inputFile(nastiness); // See c150nastyfile.h for interface

    // do an fopen on the input file
    fopenretval = inputFile.fopen(sourceName.c_str(), "rb");
    // wraps Unix fopen
    // Note rb gives "read, binary"
    // which avoids line end munging

    if (fopenretval == NULL)
    {
      throw C150Exception("File open failed.");
    }

    //
    // Read the whole file
    //
    len = inputFile.fread(buffer, 1, sourceSize);

    if (len != sourceSize)
    {
      cerr << "Error reading file " << sourceName << "  errno=" << strerror(errno) << endl;
      exit(16);
    }

    unsigned char obuf[20];

    SHA1((const unsigned char *)buffer,
         len, obuf);
    free(buffer);
    if (inputFile.fclose() != 0)
    {
      cerr << "Error closing input file " << sourceName << " errno=" << strerror(errno) << endl;
      exit(16);
    }

    // note: file must be null terminated for this to work.
    return getHexRepresentation(obuf, 20);
  }
  catch (C150Exception &e)
  {
    cerr << "nastyfiletest:copyfile(): Caught C150Exception: " << e.formattedExplanation() << endl;
    return "";
  }
}


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     writeMsg
//
//        writes a message until a correct response is given.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

void WriteHelper::writeMsg(C150NastyDgmSocket *sock, TransmissionPacket outgoing, int attempts)
{
  // bool timeout = true;
  // Continue try to send the message attempts time, or if it timeouts.
  for (int i = 0; i < attempts; i++)
  {

    memcpy(w, &outgoing, sizeof(w));
    sock->write(w, sizeof(w));

    // ssize_t readlen = sock -> read(w, sizeof(w));
    // timeout = sock -> timedout();

    // if (readlen == 0) {
    //     c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
    //     continue;
    // }
    // // While sock doesn't time out and wrong packet recieved, continue getting messages
    // while(!timeout) {
    //     TransmissionResponsePacket pckt = *(reinterpret_cast<TransmissionResponsePacket*>(w));
    //     if(outgoing.cmd == pckt.cmd && pckt.fileId == outgoing.fileId && pckt.packetId == outgoing.packetId) {
    //       return outgoing;
    //     } else {
    //       timeout = true;
    //       readlen = sock -> read(w, sizeof(w));
    //       timeout = sock -> timedout();
    //       if (readlen == 0) {
    //         c150debug->printf(C150APPLICATION,"Read zero length message, trying again");
    //         break;
    //        }
    //     }

    //   }
  }
  // throw C150Exception("Network down.");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     writeMsg
//
//        writes a message until a correct response is given.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

StartPacket WriteHelper::writeMsg(C150NastyDgmSocket *sock, StartPacket outgoing, int attempts)
{
  bool timeout = true;

  // Continue try to send the message attempts time, or if it timeouts.
  for (int i = 0; i < attempts && timeout; i++)
  {

    memcpy(w, &outgoing, sizeof(w));
    sock->write(w, sizeof(w));

    ssize_t readlen = sock->read(w, sizeof(w));
    timeout = sock->timedout();

    if (readlen == 0)
    {
      c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
      continue;
    }
    // While sock doesn't time out and wrong packet recieved, continue getting messages
    while (!timeout)
    {
      StartResponsePacket pckt = *(reinterpret_cast<StartResponsePacket *>(w));
      if (outgoing.cmd == pckt.cmd && outgoing.fileSz == pckt.fileSz)
      {
        // comparing the two filenames
        for (long unsigned int i = 0; i < 256; i++)
        {
          if (outgoing.name[i] != pckt.name[i])
          {
            timeout = true;
            break;
          }
          else if (outgoing.name[i] == '\0')
            break;
        }

        if (!timeout)
          return outgoing;
      }
      else
      {
        timeout = true;
        readlen = sock->read(w, sizeof(w));
        timeout = sock->timedout();
        if (readlen == 0)
        {
          c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
          break;
        }
      }
    }
  }
  throw C150Exception("Network down.");
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
//                     writeMsg
//
//        writes a message until a correct response is given.
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

EndToEndResponsePacket WriteHelper::writeMsg(C150NastyDgmSocket *sock, EndToEndPacket outgoing, int attempts)
{
  bool timeout = true;
  for (int i = 0; i < attempts && timeout; i++)
  {
    memcpy(w, &outgoing, sizeof(EndToEndPacket));

    // Cntinue try to send the message attempts time, or if it timeouts.
    sock->write(w, sizeof(w));

    ssize_t readlen = sock->read(w, sizeof(w));
    timeout = sock->timedout();

    if (readlen == 0)
    {
      c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
      continue;
    }
    // While sock doesn't time out and wrong packet recieved, continue getting messages.
    while (!timeout)
    {
      EndToEndResponsePacket pckt = *(reinterpret_cast<EndToEndResponsePacket *>(w));
      if (outgoing.cmd == pckt.cmd && outgoing.fileId == pckt.fileId && outgoing.packetId == pckt.packetId)
      {
        return pckt;
      }
      else
      {
        timeout = true;
        readlen = sock->read(w, sizeof(w));
        timeout = sock->timedout();
        if (readlen == 0)
        {
          c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
          break;
        }
      }
    }
  }

  throw C150Exception("Network down.");
}

ConfirmPacket WriteHelper::writeMsg(C150NastyDgmSocket *sock, ConfirmPacket outgoing, int attempts)
{
  bool timeout = true;

  // Continue try to send the message attempts time, or if it timeouts.
  for (int i = 0; i < attempts && timeout; i++)
  {

    memcpy(w, &outgoing, sizeof(w));
    sock->write(w, sizeof(w));

    ssize_t readlen = sock->read(w, sizeof(w));
    timeout = sock->timedout();

    if (readlen == 0)
    {
      c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
      continue;
    }
    // While sock doesn't time out and wrong packet recieved, continue getting messages
    while (!timeout)
    {
      ConfirmPacket pckt = *(reinterpret_cast<ConfirmPacket *>(w));
      if (pckt.cmd == outgoing.cmd)
      {
        // comparing the two filenames
        for (long unsigned int i = 0; i < 256; i++)
        {
          if (outgoing.name[i] != pckt.name[i])
          {
            timeout = true;
            break;
          }
          else if (outgoing.name[i] == '\0')
            break;
        }

        if (!timeout)
          return pckt;
      }
      else
      {
        timeout = true;
        readlen = sock->read(w, sizeof(w));
        timeout = sock->timedout();
        if (readlen == 0)
        {
          c150debug->printf(C150APPLICATION, "Read zero length message, trying again");
          break;
        }
      }
    }
  }
  throw C150Exception("Network down.");
}

Hash *newHash(unsigned char obuf[20])
{
  Hash *hash = new Hash;
  for (int i = 0; i < 20; i++)
  {
    hash->obuf[i] = obuf[i];
  }
  return hash;
}