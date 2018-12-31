
#include <fstream>
#include <iostream>

using namespace std;

#define CMD_CHECK(condition, errno) \
  if (!(condition)) { \
    cmd_error(errno); \
  }

/*
 * Class IHEX2CMD converts a file in Intel's HEX format
 * to a TRS-80 CMD executable. The conversion is primitive
 * in the sense that an IHEX record is converted to a
 * TRS-80 segment. That should be improved at some point.
 */
class IHEX2CMD {
private:
  int argc;
  char** argv;
  ifstream ihex;
  ofstream cmd;
  string rec;
  int pos;
  int entry_addr_msb;
  int entry_addr_lsb;

  void cmd_error(int errnum) {
    switch (errnum) {
    case -1:
      throw "Usage: ihex2cmd <input> <output>";
    case -2:
      throw "Input file does not exist";
    case -3:
      throw "Could not open output file";
    case -4:
      throw "Bad ihex format";
    default:
      throw "UNKNOWN ERROR";
    }
  }
  
  void parse_args() {
    CMD_CHECK(argc == 3, -1);
  }

  unsigned char next_nibble() {
    CMD_CHECK(pos != rec.length(), -4);
    char ch = rec[pos++];
    CMD_CHECK((ch >= '0' && ch <='9') || (ch >= 'A' && ch <= 'F'), -4);
    ch -= '0';
    if (ch > 9) {
      ch -= 'A' - '0' - 10;
    }
    return ch;
  }
  
  unsigned char next_byte() {
    unsigned char h = next_nibble();
    unsigned char l = next_nibble();
    return (h << 4) | l;
  }

  void parse_record() {
    char header[4];
    char len = next_byte();
    header[3] = next_byte();
    header[2] = next_byte();
    if (entry_addr_lsb == 256) {
      entry_addr_lsb = header[2];
      entry_addr_msb = header[3];
    }
    char type = next_byte();
    CMD_CHECK((type == 0) || (type == 1), -4);
    if (type == 1) {
      // Entry address
      char entry[4];
      entry[0] = 2;
      entry[1] = 2;
      entry[2] = entry_addr_lsb;
      entry[3] = entry_addr_msb;
      cmd.write(entry, 4);
      return;
    }
    // Write segment
    header[0] = 1;
    header[1] = len + 2;
    cmd.write(header, 4);
    for (int i = 0; i < len; i++) {
      char b = next_byte();
      cmd.write(&b, 1);
    }
  }    
    
  void parse_ihex() {
    while (ihex >> rec) {
      CMD_CHECK(rec.length() > 10 && rec[0] == ':', -4);
      pos = 1;
      parse_record();
    }
  }
  
public:
  IHEX2CMD(int argc, char* argv[]) {
    this->argc = argc;
    this->argv = argv;
  }

  int convert() {
    try {
      entry_addr_lsb = 256;
      parse_args();
      ihex.open(argv[1]);
      CMD_CHECK(ihex, -2);
      cmd.open(argv[2], ios::out | ios::binary);
      parse_ihex();
      ihex.close();
      cmd.close();
    } catch(const char* msg) {
      cerr << msg << endl;
      return -1;
    }
    return 0;
  }
};

int main(int argc, char* argv[])
{
  IHEX2CMD converter(argc, argv);

  return converter.convert();
}
