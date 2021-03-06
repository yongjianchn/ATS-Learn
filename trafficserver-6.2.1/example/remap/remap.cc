/** @file

  A simple remap plugin for ATS

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

  @section description
  Build this sample remap plugin using tsxs:

    $ tsxs -v -o remap.so remap.cc

  To install it:
    # tsxs -i -o remap.so
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <pwd.h>
#include <pthread.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "ts/ink_defs.h"
#include "ts/ts.h"
#include "ts/remap.h"

class remap_entry
{
public:
  static remap_entry *active_list;
  static pthread_mutex_t mutex;
  remap_entry *next;
  int argc;
  char **argv;
  remap_entry(int _argc, char *_argv[]);
  ~remap_entry();
  static void add_to_list(remap_entry *re);
  static void remove_from_list(remap_entry *re);
};

static int plugin_init_counter = 0;               /* remap plugin initialization counter */
static pthread_mutex_t remap_plugin_global_mutex; /* remap plugin global mutex */
remap_entry *remap_entry::active_list = 0;
pthread_mutex_t remap_entry::mutex; /* remap_entry class mutex */

/* ----------------------- remap_entry::remap_entry ------------------------ */
remap_entry::remap_entry(int _argc, char *_argv[]) : next(NULL), argc(0), argv(NULL)
{
  int i;

  if (_argc > 0 && _argv && (argv = (char **)TSmalloc(sizeof(char *) * (_argc + 1))) != 0) {
    argc = _argc;
    for (i    = 0; i < argc; i++)
      argv[i] = TSstrdup(_argv[i]);
    argv[i]   = NULL;
  }
}

/* ---------------------- remap_entry::~remap_entry ------------------------ */
remap_entry::~remap_entry()
{
  int i;

  if (argc && argv) {
    for (i = 0; i < argc; i++)
      TSfree(argv[i]);
    TSfree(argv);
  }
}

/* --------------------- remap_entry::add_to_list -------------------------- */
void
remap_entry::add_to_list(remap_entry *re)
{
  if (likely(re && plugin_init_counter)) {
    pthread_mutex_lock(&mutex);
    re->next    = active_list;
    active_list = re;
    pthread_mutex_unlock(&mutex);
  }
}

/* ------------------ remap_entry::remove_from_list ------------------------ */
void
remap_entry::remove_from_list(remap_entry *re)
{
  remap_entry **rre;
  if (likely(re && plugin_init_counter)) {
    pthread_mutex_lock(&mutex);
    for (rre = &active_list; *rre; rre = &((*rre)->next)) {
      if (re == *rre) {
        *rre = re->next;
        break;
      }
    }
    pthread_mutex_unlock(&mutex);
  }
}

/* ----------------------- store_my_error_message -------------------------- */
static TSReturnCode
store_my_error_message(TSReturnCode retcode, char *err_msg_buf, int buf_size, const char *fmt, ...)
{
  if (likely(err_msg_buf && buf_size > 0 && fmt)) {
    va_list ap;
    va_start(ap, fmt);
    err_msg_buf[0] = 0;
    (void)vsnprintf(err_msg_buf, buf_size - 1, fmt, ap);
    err_msg_buf[buf_size - 1] = 0;
    va_end(ap);
  }
  return retcode; /* error code here */
}

void
TSPluginInit(int argc ATS_UNUSED, const char *argv[] ATS_UNUSED)
{
  TSPluginRegistrationInfo info;
  info.plugin_name   = (char *)"remap_plugin";
  info.vendor_name   = (char *)"Apache";
  info.support_email = (char *)"";

  if (TSPluginRegister(&info) != TS_SUCCESS) {
    TSError("[remap] Plugin registration failed.");
  }
  TSDebug("debug-remap", "TSPluginInit: Remap plugin started\n");
}

// Plugin initialization code. Called immediately after dlopen() Only once!
// Can perform internal initialization. For example, pthread_.... initialization.
/* ------------------------- TSRemapInit ---------------------------------- */
TSReturnCode
TSRemapInit(TSRemapInterface *api_info, char *errbuf, int errbuf_size)
{
  fprintf(stderr, "Remap Plugin: TSRemapInit()\n");

  if (!plugin_init_counter) {
    if (unlikely(!api_info)) {
      return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapInit] - Invalid TSRemapInterface argument");
    }
    if (unlikely(api_info->size < sizeof(TSRemapInterface))) {
      return store_my_error_message(TS_ERROR, errbuf, errbuf_size,
                                    "[TSRemapInit] - Incorrect size of TSRemapInterface structure %d. Should be at least %d bytes",
                                    (int)api_info->size, (int)sizeof(TSRemapInterface));
    }
    if (unlikely(api_info->tsremap_version < TSREMAP_VERSION)) {
      return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapInit] - Incorrect API version %d.%d",
                                    (api_info->tsremap_version >> 16), (api_info->tsremap_version & 0xffff));
    }

    if (pthread_mutex_init(&remap_plugin_global_mutex, 0) ||
        pthread_mutex_init(&remap_entry::mutex, 0)) { /* pthread_mutex_init - always returns 0. :) - impossible error */
      return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapInit] - Mutex initialization error");
    }
    plugin_init_counter++;
  }
  return TS_SUCCESS; /* success */
}

// Plugin shutdown
// Optional function.
/* -------------------------- TSRemapDone --------------------------------- */
void
TSRemapDone(void)
{
  fprintf(stderr, "Remap Plugin: TSRemapDone()\n");
}

// Plugin new instance for new remapping rule.
// This function can be called multiple times (depends on remap.config)
/* ------------------------ TSRemapNewInstance --------------------------- */
TSReturnCode
TSRemapNewInstance(int argc, char *argv[], void **ih, char *errbuf, int errbuf_size)
{
  remap_entry *ri;
  int i;

  fprintf(stderr, "Remap Plugin: TSRemapNewInstance()\n");

  if (argc < 2) {
    return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapNewInstance] - Incorrect number of arguments - %d", argc);
  }
  if (!argv || !ih) {
    return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapNewInstance] - Invalid argument(s)");
  }
  // print all arguments for this particular remapping
  for (i = 0; i < argc; i++) {
    fprintf(stderr, "[TSRemapNewInstance] - argv[%d] = \"%s\"\n", i, argv[i]);
  }

  ri = new remap_entry(argc, argv);

  if (!ri) {
    return store_my_error_message(TS_ERROR, errbuf, errbuf_size, "[TSRemapNewInstance] - Can't create remap_entry class");
  }

  remap_entry::add_to_list(ri);

  *ih = (void *)ri;

  return TS_SUCCESS;
}

/* ---------------------- TSRemapDeleteInstance -------------------------- */
void
TSRemapDeleteInstance(void *ih)
{
  remap_entry *ri = (remap_entry *)ih;

  fprintf(stderr, "Remap Plugin: TSRemapDeleteInstance()\n");

  remap_entry::remove_from_list(ri);

  delete ri;
}

static volatile unsigned long processing_counter = 0; // sequential counter
static int arg_index                             = 0;

/* -------------------------- TSRemapDoRemap -------------------------------- */
TSRemapStatus
TSRemapDoRemap(void *ih, TSHttpTxn rh, TSRemapRequestInfo *rri)
{
  const char *temp;
  const char *temp2;
  int len, len2, port;
  TSMLoc cfield;
  unsigned long _processing_counter =
    ++processing_counter; // one more function call (in real life use mutex to protect this counter)

  remap_entry *ri = (remap_entry *)ih;
  fprintf(stderr, "Remap Plugin: TSRemapDoRemap()\n");

  if (!ri || !rri)
    return TSREMAP_NO_REMAP; /* TS must remap this request */

  fprintf(stderr, "[TSRemapDoRemap] From: \"%s\"  To: \"%s\"\n", ri->argv[0], ri->argv[1]);

  temp = TSUrlHostGet(rri->requestBufp, rri->requestUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Request Host(%d): \"%.*s\"\n", len, len, temp);

  temp = TSUrlHostGet(rri->requestBufp, rri->mapToUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Remap To Host: \"%.*s\"\n", len, temp);

  temp = TSUrlHostGet(rri->requestBufp, rri->mapFromUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Remap From Host: \"%.*s\"\n", len, temp);

  fprintf(stderr, "[TSRemapDoRemap] Request Port: %d\n", TSUrlPortGet(rri->requestBufp, rri->requestUrl));
  fprintf(stderr, "[TSRemapDoRemap] Remap From Port: %d\n", TSUrlPortGet(rri->requestBufp, rri->mapFromUrl));
  fprintf(stderr, "[TSRemapDoRemap] Remap To Port: %d\n", TSUrlPortGet(rri->requestBufp, rri->mapToUrl));

  temp = TSUrlPathGet(rri->requestBufp, rri->requestUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Request Path: \"%.*s\"\n", len, temp);

  temp = TSUrlPathGet(rri->requestBufp, rri->mapFromUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Remap From Path: \"%.*s\"\n", len, temp);

  temp = TSUrlPathGet(rri->requestBufp, rri->mapToUrl, &len);
  fprintf(stderr, "[TSRemapDoRemap] Remap To Path: \"%.*s\"\n", len, temp);

  // InkAPI usage case
  const char *value;

  if ((cfield = TSMimeHdrFieldFind(rri->requestBufp, rri->requestHdrp, TS_MIME_FIELD_DATE, -1)) != TS_NULL_MLOC) {
    fprintf(stderr, "We have \"Date\" header in request\n");
    value = TSMimeHdrFieldValueStringGet(rri->requestBufp, rri->requestHdrp, cfield, -1, NULL);
    fprintf(stderr, "Header value: %s\n", value);
  }
  if ((cfield = TSMimeHdrFieldFind(rri->requestBufp, rri->requestHdrp, "MyHeader", sizeof("MyHeader") - 1)) != TS_NULL_MLOC) {
    fprintf(stderr, "We have \"MyHeader\" header in request\n");
    value = TSMimeHdrFieldValueStringGet(rri->requestBufp, rri->requestHdrp, cfield, -1, NULL);
    fprintf(stderr, "Header value: %s\n", value);
  }

  // How to store plugin private arguments inside Traffic Server request processing block.
  if (TSHttpArgIndexReserve("remap_example", "Example remap plugin", &arg_index) == TS_SUCCESS) {
    fprintf(stderr, "[TSRemapDoRemap] Save processing counter %lu inside request processing block\n", _processing_counter);
    TSHttpTxnArgSet((TSHttpTxn)rh, arg_index, (void *)_processing_counter); // save counter
  }
  // How to cancel request processing and return error message to the client
  // We wiil do it each other request
  if (_processing_counter & 1) {
    char *tmp                   = (char *)TSmalloc(256);
    static int my_local_counter = 0;

    size_t len = snprintf(tmp, 255, "This is very small example of TS API usage!\nIteration %d!\nHTTP return code %d\n",
                          my_local_counter, TS_HTTP_STATUS_CONTINUE + my_local_counter);
    TSHttpTxnSetHttpRetStatus((TSHttpTxn)rh, (TSHttpStatus)((int)TS_HTTP_STATUS_CONTINUE + my_local_counter));
    TSHttpTxnErrorBodySet((TSHttpTxn)rh, tmp, len, NULL); // Defaults to text/html
    my_local_counter++;
  }
  // hardcoded case for remapping
  // You need to check host and port if you are using the same plugin for multiple remapping rules
  temp  = TSUrlHostGet(rri->requestBufp, rri->requestUrl, &len);
  temp2 = TSUrlPathGet(rri->requestBufp, rri->requestUrl, &len2);
  port  = TSUrlPortGet(rri->requestBufp, rri->requestUrl);

  if (len == 10 && !memcmp("flickr.com", temp, 10) && port == 80 && len2 >= 3 && !memcmp("47/", temp2, 3)) {
    char new_path[8192];

    // Ugly, but so is the rest of this "example"
    if (len2 + 7 >= 8192)
      return TSREMAP_NO_REMAP;

    if (TSUrlPortSet(rri->requestBufp, rri->mapToUrl, TSUrlPortGet(rri->requestBufp, rri->mapToUrl)) != TS_SUCCESS)
      return TSREMAP_NO_REMAP;

    if (TSUrlHostSet(rri->requestBufp, rri->requestUrl, "foo.bar.com", 11) != TS_SUCCESS)
      return TSREMAP_NO_REMAP;

    memcpy(new_path, "47_copy", 7);
    memcpy(&new_path[7], &temp2[2], len2 - 2);

    if (TSUrlPathSet(rri->requestBufp, rri->requestUrl, new_path, len2 + 5) == TS_SUCCESS)
      return TSREMAP_DID_REMAP;
  }

  // Failure ...
  return TSREMAP_NO_REMAP;
}

/* ----------------------- TSRemapOSResponse ----------------------------- */
void
TSRemapOSResponse(void *ih ATS_UNUSED, TSHttpTxn rh, int os_response_type)
{
  int request_id = -1;
  void *data     = TSHttpTxnArgGet((TSHttpTxn)rh, arg_index); // read counter (we store it in TSRemapDoRemap function call)

  if (data)
    request_id = *((int *)data);
  fprintf(stderr, "[TSRemapOSResponse] Read processing counter %d from request processing block\n", request_id);
  fprintf(stderr, "[TSRemapOSResponse] OS response status: %d\n", os_response_type);
}
