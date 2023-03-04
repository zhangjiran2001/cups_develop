/*  For the print judge system                                                          */
/*  The daemon process on the server side is responsible for accepting IC card tap      */
/*  information . After receiving the card tap infomation, query the job through the    */
/*  Web Server and notify the printer to print.                                         */

#include <stdio.h>
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
#include <pthread.h>

/*Type define begin*/
typedef struct ICcardReaderPackageInfo
{
        long icCardReaderId;
        int packageId;
        struct ICcardReaderPackageInfo *nextInfo;

}ICRPInfo;

typedef struct searchJobInfo
{
        long icCardReaderId;
        long icCardUserId;
        int socketFd;
        socklen_t  clientLen;
        struct sockaddr_in clientAddress;

}SJInfo;

/*Type define end*/

/* macro define begin */
#define WEB_SERVER_IP_KEY_WORD "web_server_ip"
#define WEB_SERVER_PORT_KEY_WORD "web_server_port"
#define SEARCH_JOB_URL_KEY_WORD "URL_getPrint"
#define SEARCH_JOB_URL "%s?iccard_reader_id=%ld&iccard_user_id=%ld"  //web server 's URL,the length limit is 2048 Byte
#define HTTP_MESSAGE_BUFSIZE 10240                               //http send/receive message buffer size
#define HTTP_TRANSLATE_TIMEOUT 5                                 //http translate timeout

#define ICCARD_SERVER_PORT_KEY_WORD "iccardreader_port"
#define ICCARD_MESSAGE_BUFSIZE 1024                              //iccard receive message buffer size
#define ICCARD_TAP_COMMAND_KEY "150"                             //iccard tap info received flag
#define ICCARD_LCD_MESSAGE_SRECH_JOB    "009,%05ld,%s,5,0,0"     //commad the iccard reader to show the serch job message
#define ICCARD_LCD_MESSAGE_SRECH_JOB_OK "009,%05ld,%s,5,0,0"     //commad the iccard reader to show the serch job ok message
#define ICCARD_LCD_MESSAGE_SRECH_JOB_NG "009,%05ld,%s,5,3,0"     //commad the iccard reader to show the serch job ng message
#define ICCARD_LCD_MESSAGE_PARSE_FAILD  "009,%05ld,%s,5,4,0"     //commad the iccard reader to show tparse faild message

#define MAX_PRINT_JOB_NUM 1024                                   //the max number of the print job in once time
#define JOB_ID_KEY_WORD "\"job_id\""                             //the jobID key word in Web response message
#define PRINTER_NAME_KEY_WORD "\"printer_name\""                 //the printer name's key word in web response message

#define ERROR_INFO_FILE_PATH "/var/log/cups/daemon_error.log"    //errorinfomation file's path
#define WEBSERVER_CONFIG_FILE_PATH "/var/log/cups/printInspect.config"    //webserver config file's path
/* macro define end */

/* global variable begin */
//正在查询打印作业！请不要离开！      
static char Global_Serch_Job_Message[64]    = { 
        0xD5, 0xFD, 0xD4, 0xDA, 0xB2, 0xE9, 0xD1, 0xAF, 0xB4, 0xF2, 0xD3, 0xA1, 0xD7, 0xF7,
        0xD2, 0xB5, 0x20, 0xC7, 0xEB, 0xB2, 0xBB, 0xD2, 0xAA, 0xC0, 0xEB, 0xBF, 0xAA, 0x21, 
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
//发现打印作业!  即将打印,请稍候
static char Global_Serch_Job_Ok_Message[64] = {
        0xB7, 0xA2, 0xCF, 0xD6, 0xB4, 0xF2, 0xD3, 0xA1, 0xD7, 0xF7, 0xD2, 0xB5, 0xA3, 0xA1,
        0x20, 0x20, 0x20, 0xBC, 0xB4, 0xBD, 0xAB, 0xB4, 0xF2, 0xD3, 0xA1, 0xA3, 0xAC, 0xC7,
        0xEB, 0xC9, 0xD4, 0xBA, 0xF2, 0x20 };
//没有发现该用户的打印作业                        
static char Global_Serch_Job_Ng_Message[64] = {
        0xC3, 0xBB, 0xD3, 0xD0, 0xB7, 0xA2, 0xCF, 0xD6, 0xB8, 0xC3, 0xD3, 0xC3, 0xBB, 0xA7,
        0xB5, 0xC4, 0xB4, 0xF2, 0xD3, 0xA1, 0xD7, 0xF7, 0xD2, 0xB5, 0x20, 0x20, 0x20, 0x20, 
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
//解析Web数据失败                         
static char Global_Web_Parse_Faild_Message[64] = {
        0xBD, 0xE2, 0xCE, 0xF6, 0x57, 0x65, 0x62, 0xCA, 0xFD, 0xBE, 0xDD, 0xCA, 0xA7, 0xB0, 
        0xDC, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
/* http request method */
typedef enum method{
        POST_METHOD = 0,
        PUT_METHOD,
        GET_METHOD,
        DELETE_METHOD
}RequestMethod;

/* error log file's fp global variable*/
static FILE * Global_fp;
/* time string */
#define TIME_STRING_LENGTH 32
static char Global_TimeString[TIME_STRING_LENGTH];
static char Global_WebServerIP[32];
static char Global_WebServerPort[16];
static char Global_GetPrintURL[2048];
static char Global_ICCardServerPort[16];
/* global variable end */

/*FUNCTION      :get current time and save it to time string                            */
/*INPUT         :char* time_string the point of time string to save time                */
/*OUTPUT        :char* time_string the point of time string which has saved time string */
static void getTime(char* time_string) {
        
        time_t now;
        struct tm *tm_now;

        //initalize time string
        memset(time_string, 0, TIME_STRING_LENGTH);

        //get current time
        time(&now);
        tm_now = localtime(&now);

        snprintf(time_string, TIME_STRING_LENGTH, 
                "[%4d-%02d-%02d %02d:%02d:%02d]",
                tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);

}

/*FUNCTION      :get the value string from josn                                         */
/*INPUT         :char* json_data --- json data 's point                                 */
/*INPUT         :char* key_word --- key_word 's point                                   */
/*INPUT         :char* value_buf --- value_buf 's point                                 */
/*INPUT         :int value_buf_length    value_buf_length 's length                     */
/*OUTPUT        :char* value_buf --- value_buf 's point                                 */
/*OUTPUT        :int 1:success 0:failed                                                 */
static int getValueFromJosn(char* json_data,char* key_word,char* value_buf,int value_buf_length){

        char* keyword_mark = NULL;
        char* right_braces_pos = NULL;
        char* lift_braces_pos = NULL;
        int result = 0;
        int value_len = 0;
        if(json_data == NULL || key_word== NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s Json data or key word is NULL\n",Global_TimeString);
		fflush(Global_fp); 
                return result;
        }

        //serch keyword begin
        keyword_mark = json_data;
        keyword_mark = strstr(keyword_mark,key_word);
        if(keyword_mark == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s [%s] is not found\n",Global_TimeString,key_word);
		fflush(Global_fp);
                return result;
        }

        lift_braces_pos = strchr(keyword_mark,':');
        if(lift_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s [%s]'s lift ':' is not found\n",Global_TimeString,key_word);
		fflush(Global_fp);
                return result;
        }

        lift_braces_pos = strchr(lift_braces_pos,'"');
        if(lift_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s [%s]'s lift '\"' is not found\n",Global_TimeString,key_word);
		fflush(Global_fp);
                return result;
        }

        right_braces_pos = strchr(lift_braces_pos+1,'"');
        if(right_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s [%s]'s right '\"' is not found\n",Global_TimeString,key_word);
		fflush(Global_fp);
                return result;
        }
        value_len = right_braces_pos - lift_braces_pos -1;
        if(value_len <= 1 || value_len >= value_buf_length){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s there is no [%s]'s data or data is too long . value length =%d\n",Global_TimeString,key_word,value_len);
		fflush(Global_fp);
                return result;
        }
        //key word 's value is found
        snprintf(value_buf,(size_t)(value_len+1),"%s",lift_braces_pos+1);

        result = 1;
        return result;

}

/*FUNCTION      :get webserver info from config file                                    */
/*INPUT         :nothing  input info is saved in macro varible                          */
/*OUTPUT        :nothing                                                                */
static void getWebInfoFormConfigFile() {
        
        FILE * fp;
        char file_buffer[3072];
        int ret = 0;

        memset(file_buffer, 0, sizeof(file_buffer));

        fp = fopen(WEBSERVER_CONFIG_FILE_PATH,"r");
        if(fp <= 0){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s read config file failed1 getWebInfoFormConfigFile() 's fp = %p\n",Global_TimeString,fp);
	        fflush(Global_fp);
                fclose(Global_fp);
                exit(1);
        }

        ret=fread(file_buffer,sizeof(char),sizeof(file_buffer),fp);
        if(ret <=0 ){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s read config file failed2 getWebInfoFormConfigFile() 's ret = %d\n",Global_TimeString,ret);
	        fflush(Global_fp);
                fclose(Global_fp);
                fclose(fp);
                exit(1);
        } else {
                fclose(fp);
        }

        //getTime(Global_TimeString);
        //fprintf(Global_fp,"%s ret= %d \n file content is=\n%s\n",Global_TimeString,ret,file_buffer);
        //fflush(Global_fp);

        if(getValueFromJosn(file_buffer,WEB_SERVER_IP_KEY_WORD,Global_WebServerIP,sizeof(Global_WebServerIP)) != 1){
                fclose(Global_fp);
                exit(1);  
        }

        if(getValueFromJosn(file_buffer,WEB_SERVER_PORT_KEY_WORD,Global_WebServerPort,sizeof(Global_WebServerPort)) != 1){
                fclose(Global_fp);
                exit(1);  
        }

        if(getValueFromJosn(file_buffer,ICCARD_SERVER_PORT_KEY_WORD,Global_ICCardServerPort,sizeof(Global_ICCardServerPort)) != 1){
                fclose(Global_fp);
                exit(1);  
        }

        if(getValueFromJosn(file_buffer,SEARCH_JOB_URL_KEY_WORD,Global_GetPrintURL,sizeof(Global_GetPrintURL)) != 1){
                fclose(Global_fp);
                exit(1);  
        }

        getTime(Global_TimeString);
        fprintf(Global_fp,"%s \n Global_WebServerIP=%s \nGlobal_WebServerPort=%s\nGlobal_ICCardServerPort=%s\nGlobal_GetPrintURL=%s\n",
        Global_TimeString,Global_WebServerIP,Global_WebServerPort,Global_ICCardServerPort,Global_GetPrintURL);
        fflush(Global_fp);
       
}



/*FUNCTION      :close the http connect socket                                          */
/*INPUT         :int sockfd --- socket file description                                 */
/*OUTPUT        :nothing                                                                */
static void httpSocketClose(int sockfd){

        close(sockfd);

}

/*FUNCTION      :creat a socket and connect to the WebServer                            */
/*INPUT         :nothing                                                                */
/*OUTPUT        :int  --- >=1: success ; -1: failed                                     */
static int initWebSocktAndConnect() {
        struct sockaddr_in servaddr;
        int sockfd = 0;
        int web_server_port = 0;

        sscanf(Global_WebServerPort,"%d",&web_server_port);

        //initalize sockaddr_in struct
        bzero(&servaddr, sizeof(servaddr));

        //creat a new socket        
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s web socket creat failed!\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        
        //TCP/IP - IPv4
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(web_server_port);

        //exchange IP Address
        if (inet_pton(AF_INET, Global_WebServerIP, &servaddr.sin_addr) <= 0 ){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s inet_pton IP Address error!\n",Global_TimeString);
		fflush(Global_fp);
                httpSocketClose(sockfd);
                return -1;
        }

        //set http translate timeout HTTP_TRANSLATE_TIMEOUT
        struct timeval timeout;
        timeout.tv_sec = HTTP_TRANSLATE_TIMEOUT;
        timeout.tv_usec = 0;

        if(setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s initWebSocktAndConnect() setsockopt SO_SNDTIMEO error!\n",Global_TimeString);
                fflush(Global_fp);
                httpSocketClose(sockfd);
                return -1;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) <  0) {
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s initWebSocktAndConnect() setsockopt SO_RCVTIMEO error!\n",Global_TimeString);
                fflush(Global_fp);
                httpSocketClose(sockfd);
                return -1;
        }

        //connect to web server
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s connect error!\n",Global_TimeString);
		fflush(Global_fp);
                httpSocketClose(sockfd);
                return -1;
        }
        return sockfd;

}

/*FUNCTION      :make the http request header                                           */
/*INPUT         :char* http_head_message --- http head message 's data buffer address   */
/*INPUT         :RequestMethod http_request_method --- the http request method          */
/*INPUT         :char*  URI  --- the http URL                                           */
/*OUTPUT        :char* http_head_message --- http head message 's data buffer address   */
static void makeHTTPRequestHeader(char* http_head_message,RequestMethod http_request_method,char* URI) {

        char method_string[16];
        memset(method_string, 0, 16);
        char host_string[32];
        memset(host_string,0,32);

        switch (http_request_method)
        {
        case POST_METHOD:
                snprintf(method_string,5,"POST");
                break;
        case PUT_METHOD:
                snprintf(method_string,4,"PUT");
                break;
        case GET_METHOD:
                snprintf(method_string,4,"GET");
                break;
        case DELETE_METHOD:
                snprintf(method_string,7,"DELETE");
                break;
        default:
                snprintf(method_string,4,"GET");
                break;
        }

        strcat(http_head_message,method_string);
        strcat(http_head_message," ");
        strcat(http_head_message,URI);
        strcat(http_head_message," ");
        strcat(http_head_message,"HTTP/1.1\r\n");
        snprintf(host_string,sizeof(host_string),"Host: %s\r\n",Global_WebServerIP);
        strcat(http_head_message, host_string);
        strcat(http_head_message, "Content-Type: application/x-www-form-urlencoded\r\n");
        strcat(http_head_message, "Connection: close\r\n");
        strcat(http_head_message, "Content-Length: 0\r\n\r\n");
        
}

/*FUNCTION      :send http message to the http server                                   */
/*INPUT         :int sockfd --- socket file description                                 */
/*INPUT         :char* send_message --- send message 's data buffer address             */
/*OUTPUT        :int --- >=0: success ; <0: message send error                          */
static int httpSend(int sockfd,char* send_message) {
        
        int ret = 0;
        ret = write(sockfd,send_message,strlen(send_message));
        if (ret < 0) {
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s message send error! error num is %d error message is '%s'\n",
                        Global_TimeString, errno, strerror(errno));
                fflush(Global_fp);
                
        } else {
                //message send complete!
        }

        return ret;
}

/*FUNCTION      :receive http response message from the http server                     */
/*INPUT         :int sockfd --- socket file description                                 */
/*INPUT         :char* receive_buffer --- receive message 's data buffer address        */
/*OUTPUT        :int --- >0: success ; <0: message receive error ; =0: server closed    */
static int httpReceive(int sockfd, char* receive_buffer){

        char receive_temp_buffer[512];
        int i = 0;
        int receive_lenght = 0;
        char* receive_mark = receive_buffer;

        while(1){
                
                memset(receive_temp_buffer, 0, sizeof(receive_temp_buffer));
                i= read(sockfd, receive_temp_buffer, sizeof(receive_temp_buffer)-1);
                if (i==0) {
                        //server socket is closed
                        break;

                } else if(i < 0) {
                        //time out
                        getTime(Global_TimeString);
                        fprintf(Global_fp,"%s message receive error! error num is %d error message is '%s'\n",
                        Global_TimeString, errno, strerror(errno));
                        fflush(Global_fp);
                        break;

                } else {
                        if((receive_lenght + i) > HTTP_MESSAGE_BUFSIZE) {
                                getTime(Global_TimeString);
                                fprintf(Global_fp,"%s Receive buffer is full! data droped\n",Global_TimeString);
                                fflush(Global_fp);
                                receive_lenght = -1;
                                break;
                        } else {
                                snprintf(receive_mark,(size_t)(i+1),"%s",receive_temp_buffer);
                                receive_lenght = receive_lenght + i;
                                receive_mark = receive_mark + i;
                        }
                        
                }
        }
	
        return receive_lenght;
}



/*FUNCTION      :serch the jobID from http response message                             */
/*INPUT         :char* response_message --- the response message from web server        */
/*OUTPUT        :long *job_list --- printable jobid 's list pointer                     */
/*OUTPUT        :int --- printable job's number   if it <0 means there is a error       */
static int serchJobIDformHTTPResponseMessage(char* response_message,long *job_list,char* printer_name) {
        char* keyword_mark = response_message;
        char* right_braces_pos = NULL;
        char* lift_braces_pos = NULL;
        char* comma_pos = NULL;
        char* next_comma_pos = NULL;
        char data_buf[32];
        memset(data_buf, 0, sizeof(data_buf));
        int i = 0;
        job_list[0]= 0;
        printer_name[0] = '\0';

        getTime(Global_TimeString);
        fprintf(Global_fp,"%s response_message is \n%s\n",Global_TimeString,response_message);
	fflush(Global_fp);

        //serch printer name begin
        keyword_mark = strstr(keyword_mark,PRINTER_NAME_KEY_WORD);
        if(keyword_mark == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s printer name Key word is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        right_braces_pos = strchr(keyword_mark,',');
        if(right_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s printer name's right braces is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        lift_braces_pos = strchr(keyword_mark,':');
        if(lift_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s printer name's lift braces is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        if((right_braces_pos - lift_braces_pos) == 3){

                //there is no printer name's data 
                return 0;
        }
        //printer name is found
        snprintf(printer_name,(size_t)(right_braces_pos - lift_braces_pos - 2),"%s",lift_braces_pos+2);

        //serch job id begin
        keyword_mark = response_message;
        right_braces_pos = NULL;
        lift_braces_pos = NULL;
        keyword_mark = strstr(keyword_mark,JOB_ID_KEY_WORD);
        if(keyword_mark == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s job id Key word is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        right_braces_pos = strchr(keyword_mark,']');
        if(right_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s job id's right braces is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }
        lift_braces_pos = strchr(keyword_mark,'[');
        if(lift_braces_pos == NULL){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s job id's lift braces is not found\n",Global_TimeString);
		fflush(Global_fp);
                return -1;
        }

        if((right_braces_pos - lift_braces_pos) == 1){

                //there is no job id's data ,job number is 0
                return 0;
        }

        comma_pos = lift_braces_pos + 1;//comma_pos 's initalize position is data's first position
        for(i=1; i<=MAX_PRINT_JOB_NUM; i++){
                next_comma_pos = strchr(comma_pos,',');

                //the last job id or there is only one jobid
                if((next_comma_pos == NULL) || (next_comma_pos > right_braces_pos)){
                        //get job id 's string
                        snprintf(data_buf,
                                ((right_braces_pos - comma_pos)<sizeof(data_buf))
                                ?(size_t)(right_braces_pos - comma_pos +1):(size_t)(sizeof(data_buf)),
                                "%s",comma_pos);

                        //change job id 's string to long
                        sscanf(data_buf,"%ld",&(job_list[i]));
                        break;
                }else{
                // not the last job id
                        //get job id 's string
                        snprintf(data_buf,
                                ((next_comma_pos - comma_pos)<sizeof(data_buf))
                                ?(size_t)(next_comma_pos - comma_pos +1):(size_t)(sizeof(data_buf)),
                                "%s",comma_pos);

                        //change job id 's string to long
                        sscanf(data_buf,"%ld",&(job_list[i]));

                        //initalize the varable
                        comma_pos = next_comma_pos+1;
                        memset(data_buf, 0, sizeof(data_buf));
                        next_comma_pos = NULL;                       
                }
        }
        
        //printable job num is saved in job_list[0]
        job_list[0] = i;
        return i;

}

/*FUNCTION      :get jobid's list from web server by iccardreaderID and iccarduserid    */
/*INPUT         :long iccard_reader_id --- the iccard reader's id                       */
/*INPUT         :long iccard_user_id --- the iccard user's id                           */
/*OUTPUT        :long *job_list --- printable jobid 's list pointer                     */
/*OUTPUT        :char* printer_name --- printer name 's pointer                         */
/*OUTPUT        :int --- printable job's numbers                                        */
static int getJobIDfromHTTPServer(long iccard_reader_id,long iccard_user_id,long* job_id_list,char* printer_name) {

        char http_request_message[HTTP_MESSAGE_BUFSIZE];
        memset(http_request_message, 0, sizeof(http_request_message));
        char http_response_message[HTTP_MESSAGE_BUFSIZE];
        memset(http_response_message, 0, sizeof(http_request_message));
        char url[2048];
        memset(url, 0, sizeof(url));

        int printable_job_count = 0;

        int sockfd = 0;
	sockfd = initWebSocktAndConnect();
        if(sockfd == -1){
                /*connect error*/
                return -1;
        } else {
                /*connect complete*/
        }
               
        snprintf(url,sizeof(url),SEARCH_JOB_URL,Global_GetPrintURL,iccard_reader_id,iccard_user_id);
        makeHTTPRequestHeader(http_request_message,GET_METHOD,url);

        if(httpSend(sockfd,http_request_message) < 0) {
                /*http send error and close socket*/
                httpSocketClose(sockfd);
                return -1;
        } else {
               /*send complete*/ 
        }

        if(httpReceive(sockfd,http_response_message) <= 0){
                /*http receive error*/
                httpSocketClose(sockfd);
                return -1;
        } else{
                /*receive complete*/ 
				
        }

        httpSocketClose(sockfd);

        printable_job_count = serchJobIDformHTTPResponseMessage(http_response_message,job_id_list,printer_name);

        return printable_job_count;

}

/*FUNCTION      :print job id by shell command lp                                        */
/*INPUT         :int *job_list --- printable jobid 's list pointer                       */
/*INPUT         :char* printer_name --- printer name 's pointer                          */
/*OUTPUT        :nothing                                                                 */
static void lpPrintByJobid(long* job_id_list,char* printer_name){

        long i = 0;
        int system_ret = 0;
        char commad_buffer[1024];
        
        for(i = job_id_list[0]; i>0; i--) {

                memset(commad_buffer, 0, sizeof(commad_buffer));
                snprintf(commad_buffer,sizeof(commad_buffer),"lp -d %s -H resume -i %ld",printer_name,job_id_list[i]);

                system_ret = system(commad_buffer);
                if(system_ret == -1){
                        getTime(Global_TimeString);
                        fprintf(Global_fp,"%s command exe failed result is  %d\n",Global_TimeString, system_ret);
                        fflush(Global_fp);
                } else if(system_ret == 127){
                        getTime(Global_TimeString);
                        fprintf(Global_fp,"%s sh can't exe command, command exe failed result is  %d\n",
                                                                                Global_TimeString,system_ret);
                        fflush(Global_fp);
                } else {
                        //command exe ok
                }

        }
}



/*FUNCTION      :get iccardreaderID and iccarduserid  from iccard reader's tap message  */
/*INPUT         :char* tap_info --- the iccard reader's tap message                     */
/*OUTPUT        :long iccard_reader_id --- the iccard reader's id                       */
/*OUTPUT        :long iccard_user_id --- the iccard user's id                           */
/*OUTPUT        :int* package_id --- the tapinfo package's id                           */
/*OUTPUT        :int --- parse ICCard TapInfo result 1:success/<0:faild                 */
static int parseICCardTapInfo(char* tap_info,long* reader_id,long* user_id,int* package_id){

        *reader_id = 0;
        *user_id = 0;
        char* comma_pos = NULL;
        char* next_comma_pos = NULL;

        char command[32];
        char data_pakage_str[32];
        char client_ip_address_str[32];
        char server_ip_address_str[32];
        char reader_id_str[32];
        char user_id_str[32];
        char product_code_str[64];

        //tap info format 
        //command key
        comma_pos = strchr(tap_info,',');
        if(comma_pos == NULL)return -1;
        if( (comma_pos-tap_info+1) > sizeof(command) )return -1;
        snprintf(command,(size_t)(comma_pos-tap_info+1),"%s",tap_info);

        //datapakage
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -2;
        if( (next_comma_pos-comma_pos) > sizeof(data_pakage_str) )return -2;    
        snprintf(data_pakage_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //client IP
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -3;
        if( (next_comma_pos-comma_pos) > sizeof(client_ip_address_str) )return -3;
        snprintf(client_ip_address_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //server IP
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -4;
        if( (next_comma_pos-comma_pos) > sizeof(server_ip_address_str) )return -4;
        snprintf(server_ip_address_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //readerID
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -5;
        if( (next_comma_pos-comma_pos) > sizeof(reader_id_str) )return -5;
        snprintf(reader_id_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //userID
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -6;
        if( (next_comma_pos-comma_pos) > sizeof(user_id_str) )return -6;
        snprintf(user_id_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //read header
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -7;
        //door no.
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -8;
        //close door time
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -9;
        //password type
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -10;
        //password
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -11;
        //time
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,',');
        if(next_comma_pos == NULL)return -12;
 
        //Product code
        comma_pos = next_comma_pos;
        next_comma_pos = strchr(comma_pos+1,'\0');
        if(next_comma_pos == NULL)return -13;
        if( (next_comma_pos-comma_pos) > sizeof(product_code_str) )return -13;
        snprintf(product_code_str,(size_t)(next_comma_pos-comma_pos),"%s",comma_pos+1);

        //set the user id and reader id value
        sscanf(user_id_str,"%ld",user_id);
        sscanf(reader_id_str,"%ld",reader_id);
        sscanf(data_pakage_str,"%d",package_id);
        return 1;					
}

/*FUNCTION      :used the readerid and userid to serch the printable job's id  and      */
/*               printer's name from  the web server.                                   */
/*               at the last,used lp command to print by the job id and printer name    */
/*INPUT         :SJInfo* search_job_info                                                */
/*              :long* iccard_user_id--- the user's id                                  */
/*OUTPUT        :void*                                                                  */
static void* serchJobListAndPrint(void* arg){

        SJInfo* search_job_info = (SJInfo*)arg;
        long job_id_list[MAX_PRINT_JOB_NUM];
        memset(job_id_list, 0, sizeof(job_id_list));
        char printer_name[256];
        memset(printer_name, 0, sizeof(printer_name));
        char sendbuf[256];
        memset(sendbuf, 0, sizeof(sendbuf));
        int i = 0;
        int job_numbers = 0;
        pthread_t thread_id = pthread_self();

        memset(sendbuf, 0, sizeof(sendbuf));
        snprintf(sendbuf,sizeof(sendbuf)-1,ICCARD_LCD_MESSAGE_SRECH_JOB,
                                        search_job_info->icCardReaderId,Global_Serch_Job_Message);
        i = sendto(search_job_info->socketFd,sendbuf,strlen(sendbuf)-1,0,(struct sockaddr*)&(search_job_info->clientAddress),search_job_info->clientLen);
        getTime(Global_TimeString);
        fprintf(Global_fp,"%s [Thread id:%ld]send i=%d msg=%s\n",Global_TimeString,thread_id, i,sendbuf);
        fflush(Global_fp);

        job_numbers = getJobIDfromHTTPServer(search_job_info->icCardReaderId,search_job_info->icCardUserId,job_id_list,printer_name);

        if(job_numbers > 0) {

                //if founded, send the serch successful  message to the iccard reader for lcd display 
                snprintf(sendbuf,sizeof(sendbuf),ICCARD_LCD_MESSAGE_SRECH_JOB_OK,
                                search_job_info->icCardReaderId,Global_Serch_Job_Ok_Message);
                sendto(search_job_info->socketFd,sendbuf,sizeof(sendbuf),0,(struct sockaddr*)&(search_job_info->clientAddress),search_job_info->clientLen);

                lpPrintByJobid(job_id_list,printer_name);

        } else if(job_numbers == 0) {
                //if not founded,send the serch faild  message to the iccard reader for lcd display
                snprintf(sendbuf,sizeof(sendbuf),ICCARD_LCD_MESSAGE_SRECH_JOB_NG,
                                search_job_info->icCardReaderId,Global_Serch_Job_Ng_Message);
                sendto(search_job_info->socketFd,sendbuf,sizeof(sendbuf),0,
                                (struct sockaddr*)&(search_job_info->clientAddress),search_job_info->clientLen);
        } else {
                //if job_numbers is minute value , it must be a parse error occurd
                //send the parse tapinfo failed  message to the iccard reader for lcd display
                snprintf(sendbuf,sizeof(sendbuf),ICCARD_LCD_MESSAGE_PARSE_FAILD,
                                search_job_info->icCardReaderId,Global_Web_Parse_Faild_Message);
                sendto(search_job_info->socketFd,sendbuf,sizeof(sendbuf),0,(struct sockaddr*)&(search_job_info->clientAddress),search_job_info->clientLen);
        }

        free(search_job_info);
        search_job_info = NULL;
        getTime(Global_TimeString);
        fprintf(Global_fp,"%s [Thread id:%ld] is OVER\n",Global_TimeString,thread_id);
        fflush(Global_fp);
        pthread_exit((void*)0);
        return NULL;

}


/*FUNCTION      :initalize the socket UDP server as a iccard'reader server              */
/*INPUT         :nothing                                                                */
/*OUTPUT        :int >0:succesful / <=0 faild                                           */
static int initICCardSocketServer(){
           
        int    socket_fd = 0;
        struct sockaddr_in  servaddr;
        int iccard_server_port = 0;
        sscanf(Global_ICCardServerPort,"%d",&iccard_server_port);

        //init Socket
        socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if( socket_fd == -1 ){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s create socket error: %s(errno: %d)\n",Global_TimeString, strerror(errno),errno);
		fflush(Global_fp);
                return -1;
        }
        
        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        //IP is setted with INADDR_ANY,auto config localhost IP
        servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
        servaddr.sin_port = htons(iccard_server_port);
 
        //bind the serverinfo to the socket
        if( bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1){
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s bind socket error: %s(errno: %d)\n",Global_TimeString, strerror(errno),errno);
		fflush(Global_fp);
                return -1;
        }

        return socket_fd;

}


/*FUNCTION      :accept the iccard tap message form iccard reader                       */
/*INPUT         :int sockfd --- socket file description                                 */
/*OUTPUT        :nothing                                                                */
static void acceptICCardTap(int socket_fd){

        int i = 0;
        int ret = 0;
        pthread_t tid;
        socklen_t  client_len = 0;
        long iccard_reader_id = 0;
        long iccard_user_id = 0;
        int package_id = 0;
        int parse_result = 0;

        //initilaze the iccard package info used for filter repeat message package
        ICRPInfo* iccard_package_info_head = (ICRPInfo*)malloc(sizeof(ICRPInfo));
        iccard_package_info_head->icCardReaderId = 0;
        iccard_package_info_head->packageId = 0;
        iccard_package_info_head->nextInfo = NULL;
        //icrp = icard_reader_package_info used for filter repeat message package
        ICRPInfo* icrp = NULL;
        //search job info is used by thread for arg
        SJInfo* search_job_info = NULL;

        struct sockaddr_in client_address;
        memset(&client_address, 0, sizeof(client_address));
        client_len = sizeof(client_address);

        char receive_iccard_info_buffer[ICCARD_MESSAGE_BUFSIZE];
        memset(receive_iccard_info_buffer, 0, sizeof(receive_iccard_info_buffer));
        char sendbuf[256];
        memset(sendbuf, 0, sizeof(sendbuf));

        int job_numbers = 0;

        while(1) {
		
		//receive message from a client
                //the process will be suspended when no data is received
                getTime(Global_TimeString);
                fprintf(Global_fp,"%s receive iccard tapinfo ......\n",Global_TimeString);
                fflush(Global_fp); 				
                memset(receive_iccard_info_buffer, 0, sizeof(receive_iccard_info_buffer));
                i= recvfrom(socket_fd, receive_iccard_info_buffer, sizeof(receive_iccard_info_buffer), 
                                                        MSG_WAITALL, (struct sockaddr*)&client_address, &client_len);        

                if (i==0){

                        //do nothing

                } else if( i <0 ) {
                        //receive data error occurd
                        getTime(Global_TimeString);
                        fprintf(Global_fp,"%s receive iccard tapinfo socket error (errno: %d)\n",Global_TimeString, i);
                        fflush(Global_fp);

                        //forbidden dead loop
                        break;

                } else{




                        //receive data ok
                        getTime(Global_TimeString);
                        fprintf(Global_fp,"%s receive i=%d msg=%s\n",Global_TimeString, i,receive_iccard_info_buffer);
                        fflush(Global_fp);
                        //judge the receive message content
                        if(strncmp(receive_iccard_info_buffer,ICCARD_TAP_COMMAND_KEY,sizeof(ICCARD_TAP_COMMAND_KEY)-1) == 0)
			{       
                                parse_result = parseICCardTapInfo(receive_iccard_info_buffer,&iccard_reader_id,&iccard_user_id,&package_id);
                                if(parse_result == 1) {
                                        //parse tapinfo successful

                                        //filter the repeat tapinfo
                                        icrp = iccard_package_info_head;
                                        while(icrp != NULL){
                                                if(icrp->icCardReaderId != iccard_reader_id){
                                                        if(icrp->nextInfo == NULL) {
                                                                icrp->nextInfo = (ICRPInfo*)malloc(sizeof(ICRPInfo));
                                                                icrp = icrp->nextInfo;
                                                                icrp->icCardReaderId = iccard_reader_id;
                                                                icrp->packageId = package_id;
                                                                icrp->nextInfo = NULL;
                                                                /*******search job begin*********/
                                                                //initalize SJInfo
                                                                search_job_info = (SJInfo*)malloc(sizeof(SJInfo));
                                                                search_job_info->socketFd = socket_fd;
                                                                memcpy(&(search_job_info->clientAddress),&client_address,sizeof(client_address));
                                                                search_job_info->clientLen = client_len;
                                                                search_job_info->icCardReaderId = iccard_reader_id;
                                                                search_job_info->icCardUserId = iccard_user_id;
                                                                //multiple thread
                                                                ret = pthread_create(&tid,NULL,serchJobListAndPrint,(void*)search_job_info);
                                                                if(ret != 0){
                                                                        getTime(Global_TimeString);
                                                                        fprintf(Global_fp,"%s pthread_create error:%s\n",Global_TimeString, strerror(ret));
                                                                        fflush(Global_fp);
                                                                }else {
                                                                        pthread_detach(tid);
                                                                }
                                                                /*******search job end*********/
                                                                break;
                                                        } else {
                                                                icrp = icrp->nextInfo;
                                                        }

                                                        
                                                } else {
                                                        if(icrp->packageId != package_id) {
                                                                icrp->packageId = package_id;
                                                                /*******search job begin*********/ 
                                                                //initalize SJInfo
                                                                search_job_info = (SJInfo*)malloc(sizeof(SJInfo));
                                                                search_job_info->socketFd = socket_fd;
                                                                memcpy(&(search_job_info->clientAddress),&client_address,sizeof(client_address));
                                                                search_job_info->clientLen = client_len;
                                                                search_job_info->icCardReaderId = iccard_reader_id;
                                                                search_job_info->icCardUserId = iccard_user_id;
                                                                //multiple thread
                                                                ret = pthread_create(&tid,NULL,serchJobListAndPrint,(void*)search_job_info);
                                                                if(ret != 0){
                                                                        getTime(Global_TimeString);
                                                                        fprintf(Global_fp,"%s pthread_create error:%s\n",Global_TimeString, strerror(ret));
                                                                        fflush(Global_fp);
                                                                }else {
                                                                        pthread_detach(tid);
                                                                }
                                                                /*******search job end*********/
                                                        }                                                                
                                                        break;                                                                                                                
                                                }
                                        }
                                       
                                } else {
                                        //parse tapinfo failed 
                                        getTime(Global_TimeString);
                                        fprintf(Global_fp,"%s parse tapinfo failed (errno: %d)\n",Global_TimeString, parse_result);
		                        fflush(Global_fp);
                                }
                        
                        } else {
                                //received message is not necessary
                        }
                                      
                }
                        
        }

        //free the memery
        icrp = iccard_package_info_head;
        while(icrp != NULL) {
                iccard_package_info_head = icrp;
                icrp = icrp->nextInfo;
                free(iccard_package_info_head);
        }
       
}


#if 0
/*FUNCTION      :exchange the normal process to daemon process                          */
/*INPUT         :nothing                                                                */
/*OUTPUT        :nothing                                                                */
static void initDaemon() {
        
        pid_t child1 = -1;
        pid_t child2 = -1; 
        int i = 0;
        
        //creat child1 process
        child1 = fork(); 
        if (child1 < 0) {
                //creat child1 process failed
                Global_fp = fopen(ERROR_INFO_FILE_PATH,"at+");
                getTime(Global_TimeString);
	        fprintf(Global_fp,"%s child1 fork failed\n",Global_TimeString); 
		fclose(Global_fp);
		exit(1);
	} else if (child1 > 0) { 
                //end of the father process
		exit(0); 
	} else {
                //child1 process creat OK
        }
		 
        setsid(); 
        chdir("/"); 
        umask(0); 

        for(i = 0; i < getdtablesize(); i++) { 
                close(i); 
        } 
	
        Global_fp = fopen(ERROR_INFO_FILE_PATH,"at+");

        //creat child2 process 
        child2 = fork(); 
        if (child2 < 0) {
                //creat child2 process failed
                getTime(Global_TimeString);
		fprintf(Global_fp,"%s child2 fork failed\n",Global_TimeString); 
		fclose(Global_fp);
		exit(1); 
        } else if (child2 > 0) { 
                //end of the child1 process
                exit(0); 
        } else { 
                //daemon creat successful
	}
}
#endif

/*FUNCTION      :icCardServerSubProcessStart                                            */
/*              step1 exchange the process to daemon                                    */
/*              step2 initalize ICCARDread server                                       */
/*              step3 accept the ICCARDread client 's message and                       */
/*                    parse to find ICCARDread's id and user id                         */
/*              step4 connect to the web server and serch jobid list and printer's name */
/*                    by ICCARDread's id and user id                                    */
/*              step5 print job with the jobid from jobid list and printer's name       */
/*                    by LP command                                                     */
/*INPUT         :nothing                                                                */
/*OUTPUT        :nothing                                                                */
int icCardServerSubProcessStart()
{

   
        int iccard_server_socket_fd = 0;

        //exchange the process to daemon
        //initDaemon();
        Global_fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(Global_fp <= 0) exit(1);
        //get config infomation from config file
        getWebInfoFormConfigFile();
        //initalize ICCARDread server
        iccard_server_socket_fd = initICCardSocketServer();
        getTime(Global_TimeString);
        fprintf(Global_fp,"%s initICCardSocketServer() 's ret = %d\n",Global_TimeString,iccard_server_socket_fd);
	fflush(Global_fp);
        if(iccard_server_socket_fd > 0) {
                //loop accept the iccard tap info and parse them for printing
                //if there is't any data has be accepted
                //the process will be suspended by socket's recvfrom method
                acceptICCardTap(iccard_server_socket_fd);     
        }

        //if the process run to here
        //there must be a socket error occurd
        close(iccard_server_socket_fd);
        fclose(Global_fp);
        exit(1);
}
