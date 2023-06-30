#include "WebServerConnect.h"

//char* webServerURLencode(char const *s, int len, int *new_length)
int webServerURLencode(char *source_str, int source_str_len, char *target_str,int target_str_length)
{

	unsigned  char* from = NULL;
        unsigned  char* end = NULL;
	unsigned  char* start = NULL;
        unsigned  char* to = NULL;
	unsigned  char c;
	from = (unsigned char *)source_str;
	end = (unsigned char *)source_str + source_str_len;
	start = to = (unsigned char *)target_str;
        if(target_str_length < (3*source_str_len)){
                return 0;
        }

        FILE * debug_log_fp;
        debug_log_fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(debug_log_fp == NULL){
                return 0;
        }

	unsigned char hexchars[] = "0123456789ABCDEF";

	while (from < end) {
		c = *from++;

		if (c == ' ') {
			*to++ = '+';
		} else if ((c < '0' && c != '-' && c != '.') || 
                                (c < 'A' && c > '9') || 
                                (c > 'Z' && c < 'a' && c != '_') || 
                                (c > 'z')) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = '\0';
        target_str_length = strlen(target_str);
        fprintf(debug_log_fp,"target_str= %s\n target_str_length=%d\n",target_str,target_str_length);
	fflush(debug_log_fp);
        fclose(debug_log_fp);

	return 1;

}

/*FUNCTION      :get the value string from josn                                         */
/*INPUT         :char* json_data --- json data 's point                                 */
/*INPUT         :char* key_word --- key_word 's point                                   */
/*INPUT         :char* value_buf --- value_buf 's point                                 */
/*INPUT         :int value_buf_length    value_buf_length 's length                     */
/*OUTPUT        :char* value_buf --- value_buf 's point                                 */
/*OUTPUT        :int 1:success 0:failed                                                 */
int webServerGetValueFromJosn(char* json_data,char* key_word,char* value_buf,int value_buf_length){

        char* keyword_mark = NULL;
        char* right_braces_pos = NULL;
        char* lift_braces_pos = NULL;
        int result = 0;
        int value_len = 0;

        FILE * debug_log_fp;
        debug_log_fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(debug_log_fp == NULL){
                return result;
        }
        //get current time
        char time_string[32];
        memset(time_string,0,32);
        time_t now;
        struct tm *tm_now;
        time(&now);
        tm_now = localtime(&now);
        snprintf(time_string, sizeof(time_string), 
                "[%4d-%02d-%02d %02d:%02d:%02d]",
                tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);


        //serch keyword begin
        keyword_mark = json_data;
        keyword_mark = strstr(keyword_mark,key_word);
        if(keyword_mark == NULL){
                fprintf(debug_log_fp,"%s [%s] is not found\n",time_string,key_word);
		fflush(debug_log_fp);
                fclose(debug_log_fp);
                return result;
        }

        lift_braces_pos = strchr(keyword_mark,':');
        if(lift_braces_pos == NULL){
                fprintf(debug_log_fp,"%s [%s]'s lift ':' is not found\n",time_string,key_word);
		fflush(debug_log_fp);
                fclose(debug_log_fp);        
                return result;
        }

        lift_braces_pos = strchr(lift_braces_pos,'"');
        if(lift_braces_pos == NULL){
                fprintf(debug_log_fp,"%s [%s]'s lift '\"' is not found\n",time_string,key_word);
		fflush(debug_log_fp);
                fclose(debug_log_fp);        
                return result;
        }

        right_braces_pos = strchr(lift_braces_pos+1,'"');
        if(right_braces_pos == NULL){
                fprintf(debug_log_fp,"%s [%s]'s right '\"' is not found\n",time_string,key_word);
		fflush(debug_log_fp);
                fclose(debug_log_fp);
                return result;
        }
        value_len = right_braces_pos - lift_braces_pos -1;
        if(value_len <= 1 || value_len >= value_buf_length){
                fprintf(debug_log_fp,"%s there is no [%s]'s data or data is too long . value length =%d\n",time_string,key_word,value_len);
		fflush(debug_log_fp);
                fclose(debug_log_fp);
                return result;
        }
        //key word 's value is found
        snprintf(value_buf,(size_t)(value_len+1),"%s",lift_braces_pos+1);

        result = 1;
        //fprintf(debug_log_fp,"%s value_buf=%s result=%d\n",time_string,value_buf,result);
	//fflush(debug_log_fp);
        fclose(debug_log_fp);
        return result;

}

/*FUNCTION      :get webserver info from config file                                    */
/*INPUT         :nothing  input info is saved in macro varible                          */
/*OUTPUT        :int 1:success 0:failed                                                 */
int webServerGetWebInfoFormConfigFile() {
        
        FILE * fp;
        FILE * debug_log_fp;
        char file_buffer[3072];
        //char temp_filename[2048];
        int ret = 0;
        memset(file_buffer, 0, sizeof(file_buffer));
        //memset(temp_filename, 0, sizeof(temp_filename));

        debug_log_fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(debug_log_fp == NULL){
                return 0;
        }

        //get current time
        char time_string[32];
        memset(time_string,0,32);
        time_t now;
        struct tm *tm_now;
        time(&now);
        tm_now = localtime(&now);
        snprintf(time_string, sizeof(time_string), 
                "[%4d-%02d-%02d %02d:%02d:%02d]",
                tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);

        //snprintf(temp_filename, sizeof(temp_filename), "%s/%s", ServerRoot, CONFIG_FILE_PATH);
        fp = fopen(CONFIG_FILE_PATH,"r");
        if(fp <= 0){
                fprintf(debug_log_fp,"%s read config file failed1 webServerGetWebInfoFormConfigFile() 's fp = %p\n",time_string,fp);
	        fflush(debug_log_fp);
                fclose(debug_log_fp);
                return 0;
        }

        ret=fread(file_buffer,sizeof(char),sizeof(file_buffer),fp);
        if(ret <=0 ){

                fprintf(debug_log_fp,"%s read config file failed2 webServerGetWebInfoFormConfigFile() 's ret = %d\n",time_string,ret);
	        fflush(debug_log_fp);
                fclose(debug_log_fp);
                fclose(fp);
                return 0;
        } else {
                fclose(fp);
        }


        //fprintf(debug_log_fp,"%s ret= %d \n file content is=\n%s\n",time_string,ret,file_buffer);
        //fflush(debug_log_fp);

        if(webServerGetValueFromJosn(file_buffer,SERVER_IP_KEY_WORD,Global_ServerIP,sizeof(Global_ServerIP)) != 1){
                fclose(debug_log_fp);
                return 0; 
        }

        if(webServerGetValueFromJosn(file_buffer,SERVER_PORT_KEY_WORD,Global_ServerPort,sizeof(Global_ServerPort)) != 1){
                fclose(debug_log_fp);
                return 0;  
        }

        if(webServerGetValueFromJosn(file_buffer,NEW_JOB_URL_KEY_WORD,Global_NewJobURL,sizeof(Global_NewJobURL)) != 1){
                fclose(debug_log_fp);
                return 0;  
        }

        if(webServerGetValueFromJosn(file_buffer,CHANGE_JOB_STATE_URL_KEY_WORD,Global_ChangeJobStateURL,sizeof(Global_ChangeJobStateURL)) != 1){
                fclose(debug_log_fp);
                return 0;  
        }


        fprintf(debug_log_fp,"%s \nGlobal_ServerIP=%s\nGlobal_ServerPort=%s\nGlobal_NewJobURL=%s\nGlobal_ChangeJobStateURL=%s\n",
        time_string,Global_ServerIP,Global_ServerPort,Global_NewJobURL,Global_ChangeJobStateURL);
        fflush(debug_log_fp);
        fclose(debug_log_fp);
        return 1;
       
}

/*FUNCTION      :creat a socket and connect to the WebServer                            */
/*               and notify there is a new job is created                               */
/*INPUT         :web_url_info_t* url_info                                               */
/*OUTPUT        :int  --- >=1: success ; -1: failed                                     */
int notifyWebServerToCreatNewJob(web_url_info_t* url_info) {
        struct sockaddr_in servaddr;
        int sockfd = 0;
        FILE * fp = NULL;
        int ret = 0;
        char host_string[32];
        memset(host_string,0,32);
        int web_server_port = 0;

        char time_string[32];
        memset(time_string,0,32);
        time_t now;
        struct tm *tm_now;

        if(url_info == NULL) {
                return -1;
        }

        if(webServerGetWebInfoFormConfigFile() == 0){
                return -1;
        }

        fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(fp == NULL){
                return -1;
        }

        //get current time
        time(&now);
        tm_now = localtime(&now);
        snprintf(time_string, sizeof(time_string), 
                "[%4d-%02d-%02d %02d:%02d:%02d]",
                tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);


        //request message buffer
        char http_request_message[HTTP_MESSAGE_BUFSIZE];
        memset(http_request_message, 0, HTTP_MESSAGE_BUFSIZE);
        //URL message buffer
        char url[2048];
        memset(url, 0, 2048);
        char encode_url_job_name[1024];
        memset(encode_url_job_name, 0, 1024);
        char encode_url_printer_name[1024];
        memset(encode_url_printer_name, 0, 1024);

        //receive message buffer
        char http_receive_message[1024];
        memset(http_receive_message, 0, 1024);

        char response_buf[8];
        memset(response_buf, 0, 8);

        sscanf(Global_ServerPort,"%d",&web_server_port);

        //creat a new socket        
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
                fprintf(fp,"%s notifyWebServerToCreatNewJob: web socket creat failed!\n",time_string);
		fclose(fp);
                return -1;
        }
        
        //initalize sockaddr_in struct
        bzero(&servaddr, sizeof(servaddr));
        //TCP/IP  IPv4
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(web_server_port);

        //exchange IP Address
        if (inet_pton(AF_INET, Global_ServerIP, &servaddr.sin_addr) <= 0 ){
                fprintf(fp,"%s notifyWebServerToCreatNewJob: inet_pton IP Address error!\n",time_string);
		fclose(fp);
                close(sockfd);
                return -1;
        }

        //set http translate timeout HTTP_TRANSLATE_TIMEOUT
        struct timeval timeout;
        timeout.tv_sec = HTTP_TRANSLATE_TIMEOUT;
        timeout.tv_usec = 0;

        if(setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                fprintf(fp,"%s notifyWebServerToCreatNewJob() setsockopt SO_SNDTIMEO error!\n",time_string);
                fflush(fp);
                close(sockfd);
                return -1;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
                fprintf(fp,"%s notifyWebServerToCreatNewJob() setsockopt SO_RCVTIMEO error!\n",time_string);
                fflush(fp);
                close(sockfd);
                return -1;
        }

        //connect to web server
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                fprintf(fp,"%s notifyWebServerToCreatNewJob: connect error! errornum=%d\n",time_string,errno);
		fclose(fp);
                close(sockfd);
                return -1;
        }
        
        if(webServerURLencode(url_info->job_name,strlen(url_info->job_name),encode_url_job_name,sizeof(encode_url_job_name)) == 0){
                fprintf(fp,"%s webServerURLencode job name error!\n",time_string);
		fclose(fp);
                close(sockfd);
                return -1;
        }
        if(webServerURLencode(url_info->printer_name,strlen(url_info->printer_name),encode_url_printer_name,sizeof(encode_url_printer_name)) == 0){
                fprintf(fp,"%s webServerURLencode printer_name error!\n",time_string);
		fclose(fp);
                close(sockfd);
                return -1;
        }


        snprintf(url,2048,NEW_JOB_URL,Global_NewJobURL,url_info->user_name,url_info->job_id,url_info->job_uuid,encode_url_job_name,
                 url_info->job_state,encode_url_printer_name,url_info->paper_size,url_info->media_type,
                 url_info->copies,url_info->mopies,url_info->duplex,url_info->color_mode,url_info->file_num,
                 url_info->filePath,url_info->pages_num);


        strcat(http_request_message,"POST");
        strcat(http_request_message," ");
        strcat(http_request_message,url);
        strcat(http_request_message," ");
        strcat(http_request_message,"HTTP/1.1\r\n");
        snprintf(host_string,sizeof(host_string),"Host: %s\r\n",Global_ServerIP);
        strcat(http_request_message, host_string);
        strcat(http_request_message, "Connection: close\r\n");
        strcat(http_request_message, "Content-Type: application/x-www-form-urlencoded\r\n");
        strcat(http_request_message, "Content-Length: 0\r\n\r\n");

                
        ret = write(sockfd,http_request_message,strlen(http_request_message));
        if (ret < 0) {
                
                fprintf(fp,"%s notifyWebServerToCreatNewJob: message send error! error num is %d error message is '%s'\n",
                                time_string,errno, strerror(errno));
                fclose(fp);
                close(sockfd);
                return -1;
                
        }

        fprintf(fp,"%s notifyWebServerToCreatNewJob: http_send_message=%s\n",
                                time_string,http_request_message);
        fflush(fp);       
        //message send complete! 
        memset(http_receive_message, 0, sizeof(http_receive_message));
        ret = read(sockfd,http_receive_message,sizeof(http_receive_message)-1);
        if (ret < 0) {
                
                fprintf(fp,"%s notifyWebServerToCreatNewJob: message receive error! error num is %d error message is '%s'\n",
                                time_string,errno, strerror(errno));
                fclose(fp);
                close(sockfd);
                return -1;
                
        }
        fprintf(fp,"%s notifyWebServerToCreatNewJob: http_receive_message=%s\n",
                                time_string,http_receive_message);
        fflush(fp);

        ret = webServerGetValueFromJosn(http_receive_message,RESPONSE_RESULT_KEY_WORD,response_buf,sizeof(response_buf));
        if(ret == 0){

                fprintf(fp,"%s notifyWebServerToCreatNewJob: http_receive_message prase failed\n",time_string);
                fclose(fp);
                close(sockfd);
                return -1;

        }

        if(strcmp(response_buf,"OK") != 0){
                fprintf(fp,"%s notifyWebServerToCreatNewJob: response failed response msg=%s\n",time_string,response_buf);
                fclose(fp);
                close(sockfd);
                return -1;
        }

	fclose(fp);
        close(sockfd);
        return 1;

}

/*FUNCTION      :creat a socket and connect to the WebServer                            */
/*               and notify the job state changed to the WebServer                      */
/*INPUT         :int job_id --- job id                                                  */
/*INPUT         :char* job_uuid --- job's uuid                                          */
/*INPUT         :char* job_state --- job's state                                        */
/*OUTPUT        :int  --- >=1: success ; -1: failed                                     */
int notifyWebServerToChangeJobState(int job_id,char* job_uuid,char* job_state) {
        struct sockaddr_in servaddr;
        int sockfd = 0;
        FILE * fp = NULL;
        int ret = 0;
        char host_string[32];
        memset(host_string,0,32);
        int web_server_port = 0;

        char time_string[32];
        memset(time_string,0,32);
        time_t now;
        struct tm *tm_now;
        

        if(webServerGetWebInfoFormConfigFile() == 0){
                return -1;
        }
        fp = fopen(ERROR_INFO_FILE_PATH,"at+");
        if(fp == NULL){
                return -1;
        }

        //get current time
        time(&now);
        tm_now = localtime(&now);
        snprintf(time_string, sizeof(time_string), 
                "[%4d-%02d-%02d %02d:%02d:%02d]",
                tm_now->tm_year+1900,tm_now->tm_mon+1,tm_now->tm_mday,tm_now->tm_hour,tm_now->tm_min,tm_now->tm_sec);


        //request message buffer
        char http_request_message[HTTP_MESSAGE_BUFSIZE];
        memset(http_request_message, 0, HTTP_MESSAGE_BUFSIZE);
        //URL message buffer
        char url[2048];
        memset(url, 0, 2048);

        sscanf(Global_ServerPort,"%d",&web_server_port);

        //creat a new socket        
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
                fprintf(fp,"%s notifyWebServerToChangeJobState: web socket creat failed!\n",time_string);
		fclose(fp);
                return -1;
        }
        
        //initalize sockaddr_in struct
        bzero(&servaddr, sizeof(servaddr));
        //TCP/IP  IPv4
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(web_server_port);

        //exchange IP Address
        if (inet_pton(AF_INET, Global_ServerIP, &servaddr.sin_addr) <= 0 ){
                fprintf(fp,"%s notifyWebServerToChangeJobState: inet_pton IP Address error!\n",time_string);
		fclose(fp);
                close(sockfd);
                return -1;
        }

        //set http translate timeout HTTP_TRANSLATE_TIMEOUT
        struct timeval timeout;
        timeout.tv_sec = HTTP_TRANSLATE_TIMEOUT;
        timeout.tv_usec = 0;

        if(setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
                fprintf(fp,"%s notifyWebServerToChangeJobState() setsockopt SO_SNDTIMEO error!\n",time_string);
                fflush(fp);
                close(sockfd);
                return -1;
        }

        //connect to web server
        if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0){
                fprintf(fp,"%s notifyWebServerToChangeJobState: connect error!\n",time_string);
		fclose(fp);
                close(sockfd);
                return -1;
        }
        
        snprintf(url,2048,CHANGE_JOB_STATE_URL,Global_ChangeJobStateURL,job_id,job_uuid,job_state);


        strcat(http_request_message,"POST");
        strcat(http_request_message," ");
        strcat(http_request_message,url);
        strcat(http_request_message," ");
        strcat(http_request_message,"HTTP/1.1\r\n");
        snprintf(host_string,sizeof(host_string),"Host: %s\r\n",Global_ServerIP);
        strcat(http_request_message, host_string);
        strcat(http_request_message, "Connection: close\r\n");
        strcat(http_request_message, "Content-Type: application/x-www-form-urlencoded\r\n");
        strcat(http_request_message, "Content-Length: 0\r\n\r\n");

                
        ret = write(sockfd,http_request_message,strlen(http_request_message));
        if (ret < 0) {
                
                fprintf(fp,"%s notifyWebServerToChangeJobState: message send error! error num is %d error message is '%s'\n",
                                time_string,errno, strerror(errno));
                fclose(fp);
                close(sockfd);
                return -1;
                
        } else{
                //message send complete! 
        }

	fclose(fp);
        close(sockfd);
        return 1;

}
