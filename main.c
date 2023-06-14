// Copyright (c) 2020 Cesanta Software Limited
// All rights reserved
//
// See https://mongoose.ws/tutorials/json-rpc-over-websocket/

#include "mongoose.h"

static const char *s_listen_on = "ws://10.148.0.3:8081";
static const char *s_web_root = "web_root";
static struct mg_rpc *s_rpc_head = NULL;

static char *domain_filter_list[] = {"abc.top", "xyz.top"};

static struct mg_mgr mgr;

struct domain_node {
  char domain[256];
  struct domain_node *next;
};

struct domain_node *head = NULL;

void printList(){
  struct domain_node *p = head;
   printf("\n Filter list are: [");

   //start from the beginning
   while(p != NULL) {
      printf("%s, ",p->domain);
      p = p->next;
   }
   printf("]\n");
}

int searchDomain(char *domain) {
  struct domain_node *tmp = head;
  while (tmp != NULL) {
    if (strcmp(tmp->domain, domain) == 0){
      return 1;
    }
    tmp = tmp->next;
  }
  return 0;
}

void insertDomain(char *domain){

   //create a link
   struct domain_node *nd = (struct domain_node*) malloc(sizeof(struct domain_node));
   strcpy(nd->domain, domain);
   nd->next = NULL;

   if(head == NULL) {
     head = nd;
     return;
   }

   struct domain_node *node = head;

   // point it to old first node
   while(node->next != NULL)
      node = node->next;

   //point first to new first node
   node->next = nd;
   return;
}

static void rpc_sum(struct mg_rpc_req *r) {
  double a = 0.0, b = 0.0;
  mg_json_get_num(r->frame, "$.params[0]", &a);
  mg_json_get_num(r->frame, "$.params[1]", &b);
  mg_rpc_ok(r, "%g", a + b);
}

static void rpc_mul(struct mg_rpc_req *r) {
  double a = 0.0, b = 0.0;
  mg_json_get_num(r->frame, "$.params[0]", &a);
  mg_json_get_num(r->frame, "$.params[1]", &b);
  mg_rpc_ok(r, "%g", a * b);
}

static void rpc_domain_query(struct mg_rpc_req *r) {
  char *domain = NULL;
  char *result = (char *) malloc(256 *sizeof(char));
  
  domain = mg_json_get_str(r->frame, "$.params[0]");
  printf("DEBUG: domain: %s\n", domain);

  if (searchDomain(domain) == 0){
    sprintf(result, "accept");
  }else {
    sprintf(result, "block");
  }
  printf("DEBUG2: result: %s\n", result);

  mg_rpc_ok2(r, domain, result);

  free(result);
}


static void rpc_domain_add(struct mg_rpc_req *r) {
  char *domain = NULL;
  char *result = (char *) malloc(256 *sizeof(char));
  int tmp = 0;
  
  domain = mg_json_get_str(r->frame, "$.params[0]");
  printf("DEBUG: domain_add: %s\n", domain);

  if (searchDomain(domain) == 0){
      insertDomain(domain);
      sprintf(result, "Domain '%s' is added to filter list!!", domain);
      tmp = 1;
  }else {
      sprintf(result, "Domain '%s' is existed in filter list!!", domain);
      tmp = 0;
  }

  mg_rpc_ok(r, "\"%.*s\"", (int) strlen(result), result);

  free(result);

  if (tmp == 1) {

    // Broadcast message to all connected websocket clients.

    struct mg_mgr *mgr_tmp = (struct mg_mgr *) &mgr;

    char domain_list[1024] = {0,};
    struct domain_node *p = head;
    strcat(domain_list, "[");

   //start from the beginning
   while(p != NULL) {
      strcat(domain_list, p->domain);
      strcat(domain_list, ",");
      p = p->next;
   }
   strcat(domain_list, "]");
    for (struct mg_connection *c = mgr_tmp->conns; c != NULL; c = c->next) {
      if (c->data[0] != 'W') continue;
      mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:%m}",
                 MG_ESC("method"), MG_ESC("notification"), MG_ESC("message"),  MG_ESC("New domain is added to filter list"));
      mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:%m}",
                 MG_ESC("method"), MG_ESC("notification_demo_hidden"), MG_ESC("domain"),  MG_ESC(domain));
                 }
  }
}

static void rpc_client_connect(struct mg_rpc_req *r) {
  char *host = NULL;
  char *result = (char *) malloc(256 *sizeof(char));
  
  host = mg_json_get_str(r->frame, "$.params[0]");
  printf("DEBUG: host: %s\n", host);

  sprintf(result, "update successfully");

  mg_rpc_ok(r, "\"%.*s\"", (int) strlen(result), result);

  free(result);

  struct mg_mgr *mgr_tmp = (struct mg_mgr *) &mgr;

  // Broadcast message to all connected websocket clients.
  for (struct mg_connection *c = mgr_tmp->conns; c != NULL; c = c->next) {
      if (c->data[0] != 'W') continue;
      mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:%m}",
                 MG_ESC("method"), MG_ESC("client_connect"), MG_ESC("info"),  MG_ESC(host));
  }

}

// This RESTful server implements the following endpoints:
//   /websocket - upgrade to Websocket, and implement websocket echo server
//   any other URI serves static files from s_web_root
static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_OPEN) {
    // c->is_hexdumping = 1;
  } else if (ev == MG_EV_WS_OPEN) {
    c->data[0] = 'W';  // Mark this connection as an established WS client
  } else if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/websocket")) {
      // Upgrade to websocket. From now on, a connection is a full-duplex
      // Websocket connection, which will receive MG_EV_WS_MSG events.
      mg_ws_upgrade(c, hm, NULL);
    } else {
      // Serve static files
      struct mg_http_serve_opts opts = {.root_dir = s_web_root};
      mg_http_serve_dir(c, ev_data, &opts);
    }
  } else if (ev == MG_EV_WS_MSG) {
    // Got websocket frame. Received data is wm->data
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    struct mg_iobuf io = {0, 0, 0, 512};
    struct mg_rpc_req r = {&s_rpc_head, 0, mg_pfn_iobuf, &io, 0, wm->data};
    mg_rpc_process(&r);
    if (io.buf) mg_ws_send(c, (char *) io.buf, io.len, WEBSOCKET_OP_TEXT);
    mg_iobuf_free(&io);
  }
  (void) fn_data;
}

static void timer_fn(void *arg) {
  struct mg_mgr *mgr = (struct mg_mgr *) arg;
  // Broadcast message to all connected websocket clients.
  for (struct mg_connection *c = mgr->conns; c != NULL; c = c->next) {
    if (c->data[0] != 'W') continue;
    mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%m:%m,%m:[%d,%d,%d]}",
                 MG_ESC("method"), MG_ESC("notification1"), MG_ESC("params"), 1,
                 2, 3);
  }
}

int main(void) {
  //struct mg_mgr mgr;        // Event manager
  mg_mgr_init(&mgr);        // Init event manager
  mg_log_set(MG_LL_DEBUG);  // Set log level
  //mg_timer_add(&mgr, 5000, MG_TIMER_REPEAT, timer_fn, &mgr);  // Init timer

  insertDomain("abc.top");
  insertDomain("xyz.top");

  printList();

  // Configure JSON-RPC functions we're going to handle
  mg_rpc_add(&s_rpc_head, mg_str("sum"), rpc_sum, NULL);
  mg_rpc_add(&s_rpc_head, mg_str("mul"), rpc_mul, NULL);
  mg_rpc_add(&s_rpc_head, mg_str("domain_query"), rpc_domain_query, NULL);
  mg_rpc_add(&s_rpc_head, mg_str("domain_add"), rpc_domain_add, NULL);
  mg_rpc_add(&s_rpc_head, mg_str("client_connect"), rpc_client_connect, NULL);
  mg_rpc_add(&s_rpc_head, mg_str("rpc.list"), mg_rpc_list, &s_rpc_head);

  printf("Starting WS listener on %s/websocket\n", s_listen_on);
  mg_http_listen(&mgr, s_listen_on, fn, NULL);  // Create HTTP listener
  for (;;) mg_mgr_poll(&mgr, 1000);             // Infinite event loop
  mg_mgr_free(&mgr);                            // Deallocate event manager
  mg_rpc_del(&s_rpc_head, NULL);                // Deallocate RPC handlers
  return 0;
}
