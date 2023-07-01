
#include "fs-spiffs.h"
#include "mongoose.h"
#include <stdio.h>


static const char* init_with_options_resp =
  "{\"api\": 2.1,"
  "\"cwd\":%s,"
  "\"files\":[%s],"
  "\"options\":%s}";

static const char* init_without_options_resp =
  "{\"api\": 2.1,"
  "\"cwd\":%s,"
  "\"files\":[%s]}";

static const char* dir_obj_resp =
  "{\"name\":\"Root\","
  "\"hash\":\"l0_Root\","
  "\"mime\":\"directory\","
  "\"ts\":1334163643,"
  "\"size\":12345,"
  "\"dirs\":0,"
  "\"read\":1,"
  "\"write\":1,"
  "\"locked\":0,"
  "\"volumeid\":\"l0_\","
  "\"options\":%s}";

static const char* options_resp =
  "{\"path\":\"/\","
  "\"url\":\"%s\","
  "\"separator\":\"/\","
  "\"disabled\":[\"archive\","
                "\"callback\","
                "\"chmod\","
                "\"dim\","
                "\"duplicate\","
                "\"editor\","
                "\"extract\","
                "\"file\","
                "\"get\","
                "\"info\","
                "\"mkdir\","
                "\"mkfile\","
                "\"netmount\","
                "\"parents\","
                "\"paste\","
                "\"ping\","
                "\"put\","
                "\"rename\","
                "\"resize\","
                "\"search\","
                "\"size\","
                "\"tmb\","
                "\"tree\","
                "\"url\","
                "\"zipdl\"],"
  "\"copyOverwrite\":1,"
  "\"uploadOverwrite\":1,"
  "\"uploadMaxSize\":14336,"
  "\"uploadMaxConn\":1,"
  "\"uploadMime\":{\"allow\":[],\"deny\":[],\"firstOrder\":\"deny\"}"
  "}";

static const char* tree_resp =
  "{\"tree\":[]}";

void print_mg_str(struct mg_str s)
{
  for (int i = 0; i < s.len; i++) {
    printf("%c", s.ptr[i]);
  }
}

static char* get_file_obj_list()
{
  vector<FSFile>& files = the_fs->getFileList();
  return the_fs->getFileListAsJSON(files);
}

static int process_open(struct mg_connection* c, struct mg_http_message* hm)
{
  char init[2] = "";

  int err = mg_http_get_var(&hm->query, "init", init, sizeof(init));
  if (err < 0 && err != -4 /* Not found */) {
    // Error
    return 0;
  }

  bool has_init = init[0] == '1';

  char* options;
  asprintf(&options, options_resp, "/");//"http://localhost:8000/");

  char* cwd;
  asprintf(&cwd, dir_obj_resp, options);

  char* files = get_file_obj_list();

  char* init_resp;
  if (has_init) {
    asprintf(&init_resp, init_with_options_resp, cwd, files, options);
  } else {
    asprintf(&init_resp, init_without_options_resp, cwd, files);
  }
  free(cwd);
  free(files);
  free(options);

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", init_resp);
  free(init_resp);

  return 1;
}

static int process_tree(struct mg_connection *c, struct mg_http_message* hm)
{

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", tree_resp);

  return 1;
}

static int process_ls(struct mg_connection *c, struct mg_http_message* hm)
{
  vector<string> intersect;

  struct mg_str name = mg_str("intersect\%5B\%5D");
  struct mg_str k, v;
  while (mg_split(&hm->query, &k, &v, '&')) {
    if (name.len == k.len && mg_ncasecmp(name.ptr, k.ptr, k.len) == 0) {
      char dst[v.len];
      int len = mg_url_decode(v.ptr, v.len, dst, v.len, 1);
      if (len < 0) continue;
      string val(dst, len);
      intersect.push_back(val);
    }
  }

  vector<FSFile>& files = the_fs->getFileList();
  string resp = "{\"list\":{";
  for (int i = 0; i < files.size(); i++) {
    for (int k = 0; k < intersect.size(); k++) {
      if (files.at(i).name == intersect.at(k)) {
        if (!resp.empty()) {
          resp += ",";
        }
        resp += "\"" + files.at(i).hash + "\":\"" + files.at(i).name + "\"";
      }
    }
  }
  resp += "}}";

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", resp.c_str());

  return 1;
}

static int process_rm(struct mg_connection *c, struct mg_http_message* hm)
{
  vector<string> targets;

  struct mg_str name = mg_str("targets\%5B\%5D");
  struct mg_str k, v;
  while (mg_split(&hm->query, &k, &v, '&')) {
    if (name.len == k.len && mg_ncasecmp(name.ptr, k.ptr, k.len) == 0) {
      char dst[v.len + 1];
      int len = mg_url_decode(v.ptr, v.len, dst, v.len + 1, 1);
      if (len < 0) continue;
      string val(dst, len);
      targets.push_back(val);
    }
  }

  vector<string> removed;
  the_fs->rm(targets, removed);

  string resp = "";
  for (int i = 0; i < removed.size(); i++) {
    if (resp != "") {
      resp += ",";
    }
    resp += "\"" + removed.at(i) + "\"";
  }

  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"removed\":[%s]}", resp.c_str());

  return 1;
}

static int process_upload(struct mg_connection *c, struct mg_http_message* hm)
{
  struct mg_http_part part;
  size_t ofs = 0;
  int err = 0;
  mg_str filename;
  mg_str content;
  vector<FSFile> new_files;
  
  filename.ptr = NULL;
  while ((ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0) {
    if (strncmp(part.name.ptr, "cmd", 3) == 0) {
      if (strncmp(part.body.ptr, "upload", 6) != 0) {
        break;
      }
      continue;
    }
    if (strncmp(part.name.ptr, "target", 6) == 0) {
      if (strncmp(part.body.ptr, "l0_Root", 7) != 0) {
        break;
      }
      continue;
    }
    if (strncmp(part.name.ptr, "upload[]", 8) == 0) {
      if (filename.ptr != NULL) {
        break;
      }
      filename = part.filename;
      content = part.body;
      continue;
    }
    if (strncmp(part.name.ptr, "mtime[]", 7) == 0) {
      if (filename.ptr == NULL) {
        break;
      }
      long ts = strtol(part.body.ptr, NULL, 10);
      string path(filename.ptr, filename.len);
      FSFile new_file(path, content.ptr, content.len, ts);
      the_fs->create(new_file);
      new_files.push_back(new_file);
      filename.ptr = NULL;
    }
  }

  char* json = the_fs->getFileListAsJSON(new_files);
  mg_http_reply(c, 200, "Content-Type: application/json\r\n", "{\"added\":[%s]}", json);
  free(json);

  return 1;
}

int process_file_browser_req(struct mg_connection* c, struct mg_http_message* hm)
{
  char cmd[20];

  if (strncmp(hm->method.ptr, "POST", 4) == 0) {
    // Assume upload
    return process_upload(c, hm);
  }
  if (mg_http_get_var(&hm->query, "cmd", cmd, sizeof(cmd)) < 0) {
      // Error
      return 0;
  }

  if (strcmp(cmd, "open") == 0) {
    return process_open(c, hm);
  } else if (strcmp(cmd, "tree") == 0) {
    return process_tree(c, hm);
  } else if (strcmp(cmd, "ls") == 0) {
    return process_ls(c, hm);
  } else if (strcmp(cmd, "rm") == 0) {
    return process_rm(c, hm);
  } else {
    return 0;
  }
  return 1;
}
