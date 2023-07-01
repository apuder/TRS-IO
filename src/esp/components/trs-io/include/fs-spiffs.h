#pragma once

#include <vector>
#include <string>

using namespace std;


class FSFile {
public:
  string name;
  string hash;
  const char* ptr;
  int size;
  long ts;

  void generateHash();

public:
  FSFile(string& name, const char* ptr, int size, long ts);
};

class FSInterface {
protected:
  vector<FSFile> files;
public:
  vector<FSFile>& getFileList();
  void create(FSFile file);
  void rm(vector<string>& targets, vector<string>& removed);
  char* getFileListAsJSON(vector<FSFile>& files);
  virtual void performCreate(FSFile file) = 0;
  virtual void performRm(FSFile file) = 0;
};

class FSSPIFFS : public virtual FSInterface {
public:
  FSSPIFFS();
  void performCreate(FSFile file);
  void performRm(FSFile file);
};

void init_fs_roms();

extern FSInterface* the_fs;
