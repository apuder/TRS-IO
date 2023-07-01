


#include "fs-spiffs.h"
#include <dirent.h>
#include <sys/stat.h>

#define TO_HEX(x) (char) ((x) < 10 ? (x) + '0' : ((x) + 'a' - 10))

static const char* file_obj_resp =
  "{\"name\":\"%s\","
  "\"hash\":\"%s\","
  "\"phash\":\"l0_Root\","
  "\"mime\":\"application/x-executable\","
  "\"ts\":%d,"
  "\"size\":%d,"
  //"\"dirs\":0,"
  "\"read\":1,"
  "\"write\":1,"
  "\"locked\":0}";
//  "\"volumeid\":\"l0_\"}";


FSInterface* the_fs;

FSFile::FSFile(string& name, const char* ptr, int size, long ts)
{
  this->name = name;
  this->ptr = ptr;
  this->size = size;
  this->ts = ts;
  generateHash();
}

void FSFile::generateHash()
{
  hash = "l0_";
  for (int i = 0; i < name.length(); i++) {
    uint8_t c = name[i];
    hash += TO_HEX(c >> 4);
    hash += TO_HEX(c & 0x0f);
  }
}

void FSInterface::create(FSFile file)
{
  files.push_back(file);
  performCreate(file);
}

vector<FSFile>& FSInterface::getFileList()
{
  return files;
}

char* FSInterface::getFileListAsJSON(vector<FSFile>& files)
{
  int i = 0;
  char* files_json = NULL;

  for (int i = 0; i < files.size(); i++) {
    char* file_json;
    char* tmp;

    asprintf(&file_json, file_obj_resp, files.at(i).name.c_str(),
                                        files.at(i).hash.c_str(),
                                        files.at(i).ts,
                                        files.at(i).size);
    if (files_json != NULL) {
      asprintf(&tmp, "%s,%s", files_json, file_json);
      free(files_json);
      free(file_json);
      files_json = tmp;
    } else {
      files_json = file_json;
    }
  }

  if (files_json == NULL) {
    asprintf(&files_json, " ");
  }
  return files_json;
}

void FSInterface::rm(vector<string>& targets, vector<string>& removed)
{
  vector<FSFile>::iterator it_files;
  vector<string>::iterator it_targets;
  bool file_removed;

  for (it_files = files.begin(); it_files != files.end(); ) {
    file_removed = false;
    for(it_targets = targets.begin(); it_targets != targets.end(); it_targets++) {
      if (it_files->hash == *it_targets) {
        performRm(*it_files);
        removed.push_back(*it_targets);
        targets.erase(it_targets);
        files.erase(it_files);
        file_removed = true;
        break;
      }
    }
    if (!file_removed) it_files++;
  }
}

FSSPIFFS::FSSPIFFS()
{
  struct dirent* entry;
  DIR* dp;

  dp = opendir("/roms/");
  if (dp == NULL) {
    return;
  }

  while((entry = readdir(dp)) != NULL) {
    static struct stat statbuf;
    string path = "/roms/";
    path += entry->d_name;
    if (stat(path.c_str(), &statbuf) == -1) {
      printf("stat returned -1\n");
      continue;
    }
    string name = entry->d_name;
    FSFile file(name, NULL, statbuf.st_size, statbuf.st_mtim.tv_sec);
    create(file);
  }

  closedir(dp);
}

void FSSPIFFS::performCreate(FSFile file)
{
  if (file.ptr == NULL) {
    // Nothing to do
    return;
  }
  char* path;
  asprintf(&path, "/roms/%s", file.name.c_str());
  FILE* f = fopen(path, "wb");
  free(path);
  if (f == NULL) {
    return;
  }

  int ofs = 0;
  int bw;
  int btw = file.size;

  do {
    bw = fwrite(file.ptr + ofs, 1, btw, f);
    btw -= bw;
    ofs += bw;
  } while(btw != 0);

  fclose(f);
}

void FSSPIFFS::performRm(FSFile file)
{
  char* path;

  asprintf(&path, "/roms/%s", file.name.c_str());
  remove(path);
  free(path);
}


void init_fs_roms()
{
  the_fs = new FSSPIFFS();
}