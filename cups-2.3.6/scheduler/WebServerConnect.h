#ifndef _WEBSERVERCONECT_H_
#define _WEBSERVERCONECT_H_

#include <stdio.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "cupsd.h"


/* macro define */
//#define WEB_SERVER_IP_ADDRESS "192.168.0.11"                                    //web server IP address
//#define WEB_SERVER_IP_ADDRESS "192.168.1.147"                                    //web server IP address
//#define WEB_SERVER_PORT 8280     
#define SERVER_IP_KEY_WORD "web_server_ip"
#define SERVER_PORT_KEY_WORD "web_server_port"
#define NEW_JOB_URL_KEY_WORD "URL_printSaveState" 
#define CHANGE_JOB_STATE_URL_KEY_WORD "URL_stateChange" 
#define RESPONSE_RESULT_KEY_WORD "response_result"  
#define ID_CARD_KEY_WORD "ID_Card_Num"                      


#define NEW_JOB_URL "%s?PID=%s\
&JobID=%d&JobUUID=%s&JobName=%s&JobState=%s&PrinterName=%s\
&PaperSize=%s&MediaType=%s&Copies=%s&Mopies=%s&Duplex=%s&ColorMode=%s&FileNum=%d&FilePath=%s&PagesNum=%d"    //web server 's URL

#define CHANGE_JOB_STATE_URL "%s?JobID=%d&JobUUID=%s&JobState=%s"

#define HTTP_MESSAGE_BUFSIZE 10240                                              //http send/receive message buffer size
#define HTTP_TRANSLATE_TIMEOUT 5                                                //http translate timeout
#define ERROR_INFO_FILE_PATH "/opt/casic208/cups/daemon_error.log"                   //errorinfomation file's path
#define CONFIG_FILE_PATH "/opt/casic208/cups/printInspect.config"                    //webserver config file's path

static char Global_ServerIP[32];
static char Global_ServerPort[16];
static char Global_NewJobURL[2048];
static char Global_ChangeJobStateURL[2048];
static char Global_ID[128];

typedef struct web_url_info_s		
{
  char*		  user_name;
  int       job_id;
  char*     job_uuid;
  char*     job_name;
  char*     job_state;
  char*     printer_name;
  char*     paper_size;
  char*     media_type;
  char*     copies;
  char*     mopies;
  char*     duplex;
  char*     color_mode;
  int       file_num;
  char*     filePath;
  int       pages_num;
  char*     IP_adress;
} web_url_info_t;



#define ZHANG_DEBUG_LOG  // 注释此行以禁用日志输出

#ifdef ZHANG_DEBUG_LOG
// 定义日志宏，可以接收可变数量的参数
#define ZHANG_LOG(format, ...) writeLocalDebugLog(__FILE__, __LINE__, format, ##__VA_ARGS__)
#else
#define ZHANG_LOG(format, ...) ((void)0)
#endif
void writeLocalDebugLog(const char* filename, int line, const char* format, ...);
/*FUNCTION      :creat a socket and connect to the WebServer to notify there is a new job  */
/*INPUT         :web_url_info_t* url_info                                                  */
/*OUTPUT        :int  --- >=1: success ; -1: failed                                        */
int notifyWebServerToCreatNewJob(web_url_info_t* url_info);

/*FUNCTION      :creat a socket and connect to the WebServer to notify job state is changed */
/*INPUT         :int job_id,char* job_uuid,char* job_state                                  */
/*OUTPUT        :int  --- >=1: success ; -1: failed                                         */
int notifyWebServerToChangeJobState(int job_id,char* job_uuid,char* job_state);

/*FUNCTION      :get the value string from josn                                         */
/*INPUT         :char* json_data --- json data 's point                                 */
/*INPUT         :char* key_word --- key_word 's point                                   */
/*INPUT         :char* value_buf --- value_buf 's point                                 */
/*INPUT         :int value_buf_length    value_buf_length 's length                     */
/*OUTPUT        :char* value_buf --- value_buf 's point                                 */
/*OUTPUT        :int 1:success 0:failed                                                 */
int webServerGetValueFromJosn(char* json_data,char* key_word,char* value_buf,int value_buf_length);

/*FUNCTION      :get webserver info from config file                                    */
/*INPUT         :nothing  input info is saved in macro varible                          */
/*OUTPUT        :int 1:success 0:failed                                                 */
int webServerGetWebInfoFormConfigFile();

/*FUNCTION      :encode url                                                             */
/*INPUT         :char *source_str source sring's point                                  */
/*INPUT         :char *source_str_len source sring's length                             */
/*INPUT         :char *target_str target sring's point                                  */
/*INPUT         :char *target_str_length target sring's length                          */
/*OUTPUT        :int 1:success 0:failed                                                 */
/*Attention :target_str_length must be triple of source_str_len                         */
int webServerURLencode(char *source_str, int source_str_len, char *target_str,int target_str_length);

#endif