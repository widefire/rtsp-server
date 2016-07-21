#ifndef RTSP_SERVER_H_
#define RTSP_SERVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "socketEvent.h"
#include "RTSP.h"
#include "util.h"
#include "RTP.h"
#include "configData.h"

int create_RTSP_server(char *hostName,short port,socketEventCallback callback);
void createTaskJsonServer(short port,socketEventCallback callback);
void RTSP_Callback(socket_event_t	*data);
void RTP_Callback(socket_event_t *data);
void RTCP_Callback(socket_event_t *data);
void taskJsonCallback(socket_event_t *data);

//使用UDP方式时，需要两个端口接收客户端发来的RTP、RTCP数据。
int	 create_RTP_RTCP_server(socketEventCallback RTP_callback,socketEventCallback RTCP_callback);
unsigned short  Get_RTP_Port();
unsigned short	Get_RTCP_Port();

//获取当前socket的RTSPbuffer，如果没有则新建一个;
LP_RTSP_buffer Get_RTSP_buffer(socket_t const	sock_id);
//处理RTSP信息;成功返回0;
int	RTSP_handler(LP_RTSP_buffer rtsp);
//解析TCP数据;成功返回0;
int TCP_data_parse_valid_RTSP_RTP_RTCP_data(LP_RTSP_buffer rtsp,std::list<DataPacket> &data_list);
//判断RTSPbuffer是否接收完整;成功返回0;
int RTSP_end(LP_RTSP_buffer rtsp);
//客户端socket已断开,成功返回0;
int RTSP_Close_Client(socket_t sock_id);
//是否为有效的RTSP信息,成功返回RTSP指令类型;
RTSP_CMD_STATE RTSP_validate_method(LP_RTSP_buffer rtsp);
//是否为RTCP信息;成功返回0;
int is_RTCP_data(LP_RTSP_buffer rtsp);
//根据RTSP状态和method生成需要返回的buffer
int RTSP_state_machine(LP_RTSP_buffer rtsp,RTSP_CMD_STATE method);
//发送RTSP错误信息
int RTSP_send_err_reply(int errCode,char *addon,LP_RTSP_buffer rtsp);
//解析并处理从客户端收到的OPTIONS,成功返回0;
int RTSP_OPTIONS(LP_RTSP_buffer rtsp);
//向客户端发送OPTIONS的响应消息,成功返回0;
int RTSP_OPTIONS_reply(LP_RTSP_buffer rtsp);
//解析并处理从客户端收到的DESCRIBE，成功返回0;
int RTSP_DESCRIBE(LP_RTSP_buffer rtsp);
//向客户端发送DESCRIBE的响应消息，成功返回0;
int RTSP_DESCRIBE_reply(LP_RTSP_buffer rtsp,char	*object,char *descr);
//添加输出buffer到RTSP_buffer,成功返回0;
int RTSP_add_out_buffer(char *buffer,unsigned short len,LP_RTSP_buffer rtsp);
//根据错误码返回错误字符串;
char *get_stat(int err);
//添加RTSP 时间;
void add_RTSP_timestamp(char *p);
//解析url，成功返回0，无效返回1，输入错误返回-1;
int parse_url(const char *url,char *server,size_t server_len,unsigned short &port,char *filename,size_t filename_len);
//判断是否为支持的文件类型,成功返回0:video 1:audio;
int is_supported_file_stuffix(char *p);
//生成SDP信息,成功返回0;
int generate_SDP_description(char *url,char *descr);
//生成特定格式流的SDP信息，供generate_SDP_description使用;
int generate_SDP_info(char *url,char *sdpLines,size_t sdp_line_size);
//解析并处理SETUP信息,成功返回0;
int RTSP_SETUP(LP_RTSP_buffer rtsp,LP_RTSP_session &session);
//向客户端发送SETUP响应,成功返回0;
int RTSP_SETUP_reply(LP_RTSP_buffer rtsp,LP_RTSP_session session);
//关闭一个RTSP连接,成功返回0;
int RTSP_TEARDOWN(LP_RTSP_buffer rtsp, LP_RTSP_session session);
//发送TEARDOWN响应,成功返回0;
int RTSP_TEARDOWN_reply(LP_RTSP_buffer rtsp, LP_RTSP_session session);
//解析并处理PLAY信息，成功返回0;
int RTSP_PLAY(LP_RTSP_buffer rtsp,LP_RTSP_session session);
//发送PLAY响应，成功返回0;
int RTSP_PLAY_reply(LP_RTSP_buffer rtsp,char *object,LP_RTSP_session session);
//解析并处理pause信息，成功返回0;
int RTSP_PAUSE(LP_RTSP_buffer rtsp,LP_RTSP_session session);
//发送pause响应，成功返回0;
int RTSP_PAUSE_reply(LP_RTSP_buffer rtsp, LP_RTSP_session session);

//根据当前时间和类型生成RTP时间，即video9000，audio8000;
unsigned int convert_to_RTP_timestamp(timeval tv, int freq);

//taskJson
void taskJsonCloseClient(socket_t sockId);
#endif