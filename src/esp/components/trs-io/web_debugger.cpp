#include "web_debugger.h"

#include <stdlib.h>
#include <stdbool.h>

#include "cJSON.h"
#include "mongoose.h"

static TRX_Context* ctx = NULL;
static struct mg_connection* status_conn = NULL;
static bool trx_running = true;

static TRX_MemorySegment memory_query_cache;

// static bool init_webserver(void);
static char* get_registers_json(const TRX_StatusRegistersAndFlags* registers);
static bool www_handler(struct mg_connection *conn,
                        int ev, void *ev_data, void *fn_data);

static void handleDynamicUpdate();
static bool emulation_running = false;
static int emulation_is_halting = false;
static uint32_t last_update_sent = 0;

typedef struct {
  uint16_t address;
  TRX_BREAK_TYPE type;
  bool enabled;
  bool synthetic; // TODO: NEW
} TRX_Breakpoint;

static TRX_Breakpoint* breakpoints_ = NULL;
static int max_breakpoints_ = 0;
static bool alt_single_step_mode_ = false;

static TRX_CONTROL_TYPE next_async_action = TRX_CONTROL_TYPE_NOOP;

// public
// Create context with default values. Caller own object.
TRX_Context* get_default_trx_context() {
  TRX_Context* ctx =  new TRX_Context;
  ctx->system_name = "";
  ctx->model = UNDEFINED;
  ctx->rom_version = 0;
  ctx->capabilities.memory_range.start = 0;
  ctx->capabilities.memory_range.length = 0xFFFF;
  ctx->capabilities.max_breakpoints = 128;
  ctx->capabilities.alt_single_step_mode = false;
  ctx->control_callback = NULL;
  ctx->read_memory = NULL;
  ctx->write_memory = NULL;
  ctx->breakpoint_callback = NULL;
  ctx->remove_breakpoint_callback = NULL;
  ctx->get_resource = NULL;
  ctx->get_state_update = NULL;
  ctx->set_pc = NULL;
  return ctx;
}

// public
bool init_trs_xray(TRX_Context* ctx_param) {
  if (ctx != NULL) {
    puts("[TRX] ERROR: Already initialized.");
    return false;
  }
	// if (!init_webserver()) {
  //   puts("[TRX] ERROR: Cannot init webserver. Aborting initialization.");
  //   return false;
  // }
  max_breakpoints_ = ctx_param->capabilities.max_breakpoints;
  alt_single_step_mode_ = ctx_param->capabilities.alt_single_step_mode;
  last_update_sent = clock();
  ctx = ctx_param;

  breakpoints_ = new TRX_Breakpoint[max_breakpoints_];
  for (int id = 0; id < max_breakpoints_; ++id) {
    breakpoints_[id].address = 0;
    breakpoints_[id].type = TRX_BREAK_PC;
    breakpoints_[id].enabled = false;
  }

  // Pre-allocate for performance to max required size.
  memory_query_cache.data = (uint8_t*) malloc(sizeof(uint8_t) * ctx->capabilities.memory_range.length);
  return true;
}

// public
bool trx_handle_http_request(struct mg_connection *c,
                             int event, void *eventData, void *fn_data) {
  return www_handler(c, event, eventData, fn_data);
}

// public
void trx_waitForExit() {
  // int threadReturnValue;
  // SDL_WaitThread(thread, &threadReturnValue);
}

// public
void trx_shutdown() {
  puts("[TRX] Shutting down Web debugger");
  trx_running = false;
}

static void inject_demo_program(void) {
  const uint16_t START = 0x8000;
  // This is the screen fill demo, shown at Tandy Assembly 2021.
  uint8_t prog[15] = { 0x21, 0x00, 0x3c, 0x01, 0x00, 0x04, 0x36, 0xbf, 0x23,
                       0x0b, 0x78, 0xb1, 0x20, 0xf8, 0xc9 };
  for (int i = 0; i < 15; i++) {
    ctx->write_memory(START + i, prog[i]);
  }
  ctx->set_pc(0x8000);
}

static void send_update_to_web_debugger() {
  if (status_conn == NULL) return;

  TRX_SystemState state;
  ctx->get_state_update(&state);

  // Send registers.
  char* message = get_registers_json(&state.registers);
  mg_ws_send(status_conn, message, strlen(message), WEBSOCKET_OP_TEXT);
  free(message);
}

void get_memory_segment(int start, int length,
                        TRX_MemorySegment* segment,
                        bool force_full_update) {
  // Only send a range of data that changed (within the given params).
  static uint8_t* previous_memory_ = NULL;
  if (previous_memory_ == NULL) {
      previous_memory_ = (uint8_t*) malloc(sizeof(uint8_t) * ctx->capabilities.memory_range.length);
  }

  int start_actual = start;
  if (!force_full_update) {
    for (int i = start; i < start + length; ++i) {
      int data = ctx->read_memory(i);
      if (previous_memory_[i - start] != data) {
        start_actual = i;
        break;
      }
    }
  }

  int length_actual = length;
  if (!force_full_update) {
    for (int i = start + length - 1; i >= start; --i) {
      int data = ctx->read_memory(i);
      if (previous_memory_[i - start] != data) {
        length_actual = (i - start_actual) + 1;
        break;
      }
    }
  }
  segment->range.start = start_actual;
  segment->range.length = length_actual;
  for (int i = start_actual; i < start_actual + length_actual; ++i) {
    int data = ctx->read_memory(i);
    segment->data[i - start_actual] = data;
    previous_memory_[i - start] = data;
  }
}

// Params: [addr]/[value]"
static void set_memory_segment(const char* params) {
  int delim_pos = strchr(params, '/') - params;
  char addr_str[delim_pos + 1];
  memcpy(addr_str, params, delim_pos);
  addr_str[delim_pos] = '\0';
  int addr = atoi(addr_str);

  int substr_length = strlen(params) - delim_pos;
  char value_str[substr_length];
  memcpy(value_str, params + delim_pos + 1, substr_length - 1);
  value_str[substr_length - 1] = '\0';
  int value = atoi(value_str);
  ctx->write_memory(addr, value);
}

// Params: [start]/[length], e.g. "0/65536"
static void send_memory_segment(const char* params) {
  if (status_conn == NULL) return;
  // printf("TRX: MemorySegment request: '%s'.", params);

  int param_start = 0;
  int param_length = 0xFFFF;

  bool force_update = true; //strcmp("force_update", params) == 0;

  if (!force_update) {
    // Extract parameters
    int delim_pos = strchr(params, '/') - params;
    char param_start_str[delim_pos + 1];
    memcpy(param_start_str, params, delim_pos);
    param_start_str[delim_pos] = '\0';
    param_start = atoi(param_start_str);

    int substr_length = strlen(params) - delim_pos;
    char param_length_str[substr_length];
    memcpy(param_length_str, params + delim_pos + 1, substr_length - 1);
    param_length_str[substr_length - 1] = '\0';
    param_length = atoi(param_length_str);
    // printf("Parameters: start(%d) length(%d)\n", param_start, param_length);
  }

  // Limit to range supported by SUT.
  if (param_start < ctx->capabilities.memory_range.start) {
    param_start = ctx->capabilities.memory_range.start;
  }

  if (param_length > ctx->capabilities.memory_range.length) {
    param_length = ctx->capabilities.memory_range.length;
  }

  get_memory_segment(param_start, param_length, &memory_query_cache,
                     force_update);
  const TRX_MemorySegment* seg = &memory_query_cache;

  // // Add start metadata.
  uint8_t* data_to_send = (uint8_t*) malloc(sizeof(uint8_t) * (seg->range.length + 2));
  uint8_t param_start_1 = (seg->range.start & 0xFF00) >> 8;
  uint8_t param_start_2 = seg->range.start & 0x00FF;
  data_to_send[0] = param_start_1;
  data_to_send[1] = param_start_2;
  // printf("Start param pieces: %d %d\n", param_start_1, param_start_2);
  memcpy(data_to_send + 2, seg->data, seg->range.length);

  // Send registers.
  mg_ws_send(status_conn, (const char*)data_to_send, seg->range.length + 2, WEBSOCKET_OP_BINARY);
  free(data_to_send);
}

// Params: [address in decimal]. e.g. "1254"
static void add_breakpoint(const char* params, TRX_BREAK_TYPE type) {
  int addr = atoi(params);
  if (addr == 0 && strcmp("0", params) != 0) {
    puts("[TRX] Error: Cannot parse address.");
    return;
  }

  int id = 0;
  for (id = 0; id < max_breakpoints_; ++id) {
    if (!breakpoints_[id].enabled) break;
  }
  if (id >= max_breakpoints_) {
    puts("[TRX] Error: Too many breakpoints.");
    return;
  }

  breakpoints_[id].address = addr;
  breakpoints_[id].type = type;
  breakpoints_[id].enabled = true;
  ctx->breakpoint_callback(id, addr, type);
  send_update_to_web_debugger();
}

static void remove_breakpoint_with_id(int id) {
  if (id >= max_breakpoints_) {
    puts("[TRX] Error: Breakpoint ID too large.");
    return;
  }
  breakpoints_[id].enabled = false;
  ctx->remove_breakpoint_callback(id);
  send_update_to_web_debugger();
}

static void remove_breakpoint(const char* params) {
  int id = atoi(params);
  if (id == 0 && strcmp("0", params) != 0) {
    puts("[TRX] Error: Cannot parse breakpoint ID.");
    return;
  }
  remove_breakpoint_with_id(id);
}

// TODO: Should only do this for synthetic breakpoints.
static void clear_breakpoints() {
  for (int id = 0; id < max_breakpoints_; ++id) {
    if (breakpoints_[id].enabled) {
      remove_breakpoint_with_id(id);
    }
  }
  // send_update_to_web_debugger();
}


static void key_event(const char* params) {
  printf("Key Event: '%s'\n", params);
  // bool down = params[0] == '1';
  // bool shift = params[2] == '1';
  // const char* key = params + 4;
  // TODO: Pipe into SDL keyboard queue.
}

static bool handle_http_request(struct mg_connection *conn,
                                struct mg_http_message* message) {
  // Note: We don't host the html/js/css files here as they are took big
  //       for the embedded RAM.
  if (mg_http_match_uri(message, "/channel")) {
		mg_ws_upgrade(conn, message, NULL);
		status_conn = conn;
  } else {
    // Resource not found.
    // mg_http_reply(conn, 404, "Content-Type: text/html\r\nConnection: close\r\n", "");
    return false;
  }
  return true;
}

static char* get_registers_json(const TRX_StatusRegistersAndFlags* regs) {
	  cJSON* json = cJSON_CreateObject();

    cJSON* context = cJSON_CreateObject();
    cJSON_AddStringToObject(context, "system_name", ctx->system_name);
    cJSON_AddNumberToObject(context, "model", ctx->model);
    cJSON_AddBoolToObject(context, "running", emulation_running);
    cJSON_AddBoolToObject(context, "alt_single_step_mode",
                          ctx->capabilities.alt_single_step_mode);
    cJSON_AddItemToObject(json, "context", context);

    cJSON* breaks = cJSON_CreateArray();
    for (int i = 0; i < max_breakpoints_; ++i) {
      if (!breakpoints_[i].enabled) continue;
      cJSON* breakpoint = cJSON_CreateObject();
      cJSON_AddNumberToObject(breakpoint, "id", i);
      cJSON_AddNumberToObject(breakpoint, "address", breakpoints_[i].address);
      cJSON_AddNumberToObject(breakpoint, "type", breakpoints_[i].type);
      cJSON_AddItemToArray(breaks, breakpoint);
    }
    cJSON_AddItemToObject(json, "breakpoints", breaks);

    cJSON* registers = cJSON_CreateObject();
    cJSON_AddNumberToObject(registers, "pc", regs->pc);
    cJSON_AddNumberToObject(registers, "sp", regs->sp);
    cJSON_AddNumberToObject(registers, "af", regs->af);
    cJSON_AddNumberToObject(registers, "bc", regs->bc);
    cJSON_AddNumberToObject(registers, "de", regs->de);
    cJSON_AddNumberToObject(registers, "hl", regs->hl);
    cJSON_AddNumberToObject(registers, "af_prime", regs->af_prime);
    cJSON_AddNumberToObject(registers, "bc_prime", regs->bc_prime);
    cJSON_AddNumberToObject(registers, "de_prime", regs->de_prime);
    cJSON_AddNumberToObject(registers, "hl_prime", regs->hl_prime);
    cJSON_AddNumberToObject(registers, "ix", regs->ix);
    cJSON_AddNumberToObject(registers, "iy", regs->iy);
    cJSON_AddNumberToObject(registers, "i", regs->i);
    cJSON_AddNumberToObject(registers, "r_1", regs->r);
    cJSON_AddNumberToObject(registers, "r_2", (regs->r7 & 0x7f));

    cJSON_AddNumberToObject(registers, "z80_t_state_counter", regs->t_count);
    cJSON_AddNumberToObject(registers, "z80_clockspeed", regs->clock_mhz);
		cJSON_AddNumberToObject(registers, "z80_iff1", regs->iff1);
    cJSON_AddNumberToObject(registers, "z80_iff2", regs->iff2);
    cJSON_AddNumberToObject(registers, "z80_interrupt_mode", regs->interrupt_mode);

    cJSON_AddItemToObject(json, "registers", registers);

    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return str;
}

static void on_frontend_message(const char* msg) {
  if (strcmp("ping", msg) == 0) {
    // Do nothing
  } else if (strcmp("action/refresh", msg) == 0) {
    send_update_to_web_debugger();
  } else if (strcmp("action/step", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_STEP);
    send_update_to_web_debugger();
  } else if (strcmp("action/continue", msg) == 0) {
    // // Running is done asynchronously to not block the main TRX thread.
    // next_async_action = TRX_CONTROL_TYPE_CONTINUE;
    // For TRS_IO we call continue directly.
    ctx->control_callback(TRX_CONTROL_TYPE_CONTINUE);
  } else if (strcmp("action/stop", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_HALT);
  } else if (strcmp("action/soft_reset", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_SOFT_RESET);
  } else if (strcmp("action/hard_reset", msg) == 0) {
    ctx->control_callback(TRX_CONTROL_TYPE_HARD_RESET);
  } else if (strncmp("action/get_memory", msg, 17) == 0) {
    send_memory_segment(msg + 18);
  } else if (strncmp("action/set_memory", msg, 17) == 0) {
    set_memory_segment(msg + 18);
  } else if (strncmp("action/add_breakpoint/pc", msg, 24) == 0) {
    add_breakpoint(msg + 25, TRX_BREAK_PC);
  } else if (strncmp("action/add_breakpoint/mem", msg, 25) == 0) {
    add_breakpoint(msg + 26, TRX_BREAK_MEMORY);
  } else if (strncmp("action/remove_breakpoint", msg, 24) == 0) {
    remove_breakpoint(msg + 25);
  } else if (strcmp("action/clear_breakpoints", msg) == 0) {
    clear_breakpoints();
  } else if (strncmp("action/key_event", msg, 16) == 0) {
    key_event(msg + 17);
  } else if (strncmp("action/inject_demo", msg, 16) == 0) {
    inject_demo_program();
  } else {
    printf("[TRX] WARNING: Unknown message: '%s'\n", msg);
  }
}

static bool www_handler(struct mg_connection *conn,
                        int ev, void *ev_data, void *fn_data) {
  if (!trx_running) {
    return false;
  }
  switch(ev) {
    case MG_EV_HTTP_MSG: {
      return handle_http_request(conn, (struct mg_http_message*) ev_data);
    }
    case MG_EV_WS_MSG: {
      static char message[50];
      struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
      strncpy(message, wm->data.ptr, wm->data.len);
      message[wm->data.len] = '\0';
      on_frontend_message(message);
      break;
    }
    case MG_EV_CLOSE: {
      if (conn == status_conn) {
        status_conn = NULL;
      }
      break;
    }
    default: {
      return false;
    }
	}
  return true;
}

// FIXME
static void handleDynamicUpdate() {
  // // We want to send one last update when the running emulation shut down so
  // // that the frontend has the latest state.
  // if (!emulation_is_halting && !emulation_running) return;

  // if (emulation_is_halting) {
  //   puts("[TRX] Halting emulation; sending one more update.");
  // }
  // uint32_t now_millis = SDL_GetTicks();
  // uint32_t diff_millis = now_millis - last_update_sent;

  // if (diff_millis < 40 && !emulation_is_halting) return;
  // send_update_to_web_debugger();
  // send_memory_segment("0/65536");
  // last_update_sent = now_millis;
  // emulation_is_halting = false;
}