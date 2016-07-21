#include "H264VideoFileStreamSource.h"
#include "task_stream_stop.h"
#include "task_stream_file.h"
#include "task_RTCP_message.h"
#include "h264FileParser.h"
#include "util.h"
#include "configData.h"
#include "taskSchedule.h"
#include "taskSendRtspResbData.h"


#include <chrono>
#include <thread> 

H264VideoFileStreamSource::H264VideoFileStreamSource(const char *file_name, LP_RTSP_session session) :m_RTSP_socket_id(-1),
m_sps(0), m_sps_len(0), m_pps(0), m_pps_len(0), m_video_width(0),
m_video_height(0), m_fps(24), m_frame_data(0), m_frame_size(0), m_tmp_buf_in(0),
m_tmp_size(s_base_frame_size), m_frame_container_size(s_base_frame_size),
m_session(0), m_send_end(false),
m_seq(0), m_seq_num(0), m_rtp_socket(-1), m_rtcp_socket(-1), m_rtp_pkg_counts(0),
m_rtp_data_size(0), m_hasCalledDelete(false)
{
	std::string fullFileName = configData::getInstance().getFlvSaveDir() + file_name;
	m_file_reader=new localFileReader(fullFileName.c_str());
	m_session = deepcopy_RTSP_session(session);
	initStreamFile();
}

H264VideoFileStreamSource::~H264VideoFileStreamSource()
{
	m_hasCalledDelete = true;
	shutdown();
}

void H264VideoFileStreamSource::shutdown()
{
	//先停止线程，再释放资源;
	m_send_end = true;

	if (m_thread_send.joinable())
	{
		m_thread_send.join();
	}

	if (true)
	{
		m_mutex_send.lock();
		m_mutex_send.unlock();
	}
	
	if (m_rtp_socket>=0)
	{
		closeSocket(m_rtp_socket);
		m_rtp_socket = -1;
	}
	if (m_rtcp_socket>=0)
	{
		closeSocket(m_rtcp_socket);
		m_rtcp_socket = -1;
	}

	if (m_file_reader)
	{
		delete m_file_reader;
		m_file_reader=0;
	}
	if (m_sps)
	{
		delete []m_sps;
		m_sps=0;
	}
	if (m_pps)
	{
		delete []m_pps;
		m_pps=0;
	}
	if (m_frame_data)
	{
		free(m_frame_data);
		m_frame_data=0;
	}

	if (m_tmp_buf_in)
	{
		free(m_tmp_buf_in);
		m_tmp_buf_in=0;
	}
	if (m_session)
	{
		delete m_session;
		m_session = 0;
	}

}

int H264VideoFileStreamSource::startStream(taskScheduleObj *task_obj)
{
	int iRet=0;
	do
	{
		if (!task_obj||task_obj->getType()!=schedule_start_rtsp_file)
		{
			iRet = -1;
			break;
		}
		m_hasCalledDelete = false;
		if (m_sps_len<=0||m_pps_len<=0)
		{
			iRet=-1;
			break;
		}
		taskStreamFile *task_real=static_cast<taskStreamFile*>(task_obj);
		if (!m_session || m_session->session_id != task_real->get_RTSP_session()->session_id ||
			m_session->ssrc != task_real->get_RTSP_session()->ssrc)
		{
			iRet = -1;
			break;
		}
		m_RTSP_socket_id=task_real->get_RTSP_buffer()->sock_id;
		memcpy(&m_sock_addr, &task_real->get_RTSP_buffer()->sock_addr, sizeof m_sock_addr);
		if (task_real->get_RTSP_buffer()->session_list.size()<=0)
		{
			iRet = -1;
			break;
		}
		if (m_session)
		{
			delete m_session;
			m_session = nullptr;
		}
		m_session = deepcopy_RTSP_session(task_real->get_RTSP_session());
		LOG_INFO(configData::getInstance().get_loggerId(),"begin stream:ssrc:" << m_session->ssrc);

		if (m_session->transport_type==RTSP_OVER_UDP)
		{
			if (m_rtp_socket==0)
			{

				m_rtp_socket = socketCreateUdpClient();
			}
			if (m_rtcp_socket==0)
			{

				m_rtcp_socket = socketCreateUdpClient();
			}
		}
		m_file_reader->seekFile(0);
		std::thread hSendThread(sendThread, this);
		m_thread_send = std::move(hSendThread);
	} while (0);
	return iRet;
}

int H264VideoFileStreamSource::stopStream(taskScheduleObj *task_obj)
{
	int iRet=0;
	do
	{
		m_send_end = true;
	} while (0);
	return iRet;
}


int H264VideoFileStreamSource::notify_stoped()
{
	int iRet=0;
	do
	{
		if (m_hasCalledDelete)
		{
			break;
		}
		taskStreamStop *stopTask = new taskStreamStop(m_session->session_id);
		//taskSchedule::getInstance().addTaskObj(stopTask);
		taskSchedule::getInstance().addTaskObj(stopTask);
	} while (0);
	return iRet;
}

void H264VideoFileStreamSource::sendThread(void * lparam)
{
	H264VideoFileStreamSource *stream_source = (H264VideoFileStreamSource*)lparam;
	//控制时间;
	timeval time_tmp;
	unsigned int remainder = 0;
	unsigned int ms_now = 0;
	unsigned int ms_last = 0;
	unsigned int ms_next = 0;
	unsigned int ms_delta = 0;

	remainder = 1000 % stream_source->m_fps;
	ms_delta = 1000 / stream_source->m_fps;
	//发送帧;
	gettimeofday(&time_tmp, 0);
	stream_source->m_rtcp_time = time_tmp;
	ms_now = ms_last = time_tmp.tv_sec * 1000 + time_tmp.tv_usec / 1000;
	unsigned long ms_delta_real = ms_delta;
	int send_counts = 0;
	ms_next = ms_now + ms_delta;
	std::lock_guard<std::mutex> guard(stream_source->m_mutex_send);
	stream_source->m_file_reader->seekFile(0);
	while (!stream_source->m_send_end)
	{
		if (++send_counts==stream_source->m_fps)
		{
			ms_next += (ms_delta + remainder);
			send_counts = 0;
		}else
		{
			ms_next += ms_delta;
		}
		gettimeofday(&time_tmp, 0);
		ms_now = time_tmp.tv_sec * 1000 + time_tmp.tv_usec / 1000;
		//高延时情况导致现在时间大于下一帧时间;
		if (ms_now>ms_next)
		{
			ms_next = ms_now;
		}else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(ms_next - ms_now));
		}
		if (0 != stream_source->send_frame())
		{
 			stream_source->m_send_end = true;
			break;
		}
	}
	stream_source->notify_stoped();
	if (stream_source->m_hasCalledDelete)
	{

		LOG_INFO(configData::getInstance().get_loggerId(), "pause stream thread,sock id:" << stream_source->m_RTSP_socket_id);
	}
	else
	{

		LOG_INFO(configData::getInstance().get_loggerId(), "shutdown stream thread,sock id:" << stream_source->m_RTSP_socket_id);
	}
}

int H264VideoFileStreamSource::send_frame()
{
	int iRet = 0;
	int sendRet = 0;
	int h264_tcp_header_size = 4;
	int h264_tcp_data_size = SOCKET_PACKET_SIZE - h264_tcp_header_size;
	std::list<DataPacket>	pkgs;
	sockaddr_in	remote_addr;
	int remote_addr_len = sizeof(sockaddr_in);
	memset(&remote_addr, 0, sizeof remote_addr);
	do
	{
		if (!m_session)
		{
			iRet = -1;
			break;
		}

		m_mutexRtspSend.lock();
		for (auto i : m_vectorRtspSendData)
		{
			socketSend(m_RTSP_socket_id, i.c_str(), i.size());
		}
		m_vectorRtspSendData.clear();
		m_mutexRtspSend.unlock();

		if (m_session->transport_type==RTSP_OVER_TCP)
		{
			//RTP

			iRet=generate_sendable_frame(pkgs, h264_tcp_data_size);
			if (iRet!=0)
			{
				//Just send bye;
				RTCP_process Rtcp_process;
				DataPacket bye_packet;
				bye_packet.data = 0;
				Rtcp_process.generate_Bye_Buf(bye_packet, m_rtcp_ssrc);
				char	tcp_header[4];
				tcp_header[0] = '$';
				tcp_header[1] = m_session->RTCP_channel;
				tcp_header[2] = (bye_packet.size >> 8);
				tcp_header[3] = (bye_packet.size & 0xff);
				sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);
				sendRet = socketSend(m_RTSP_socket_id, bye_packet.data, bye_packet.size);
				delete[]bye_packet.data;
				bye_packet.data = 0;
				break;
			}
			//添加TCP头,发送数据;
			std::list<DataPacket>::iterator it;
			for (it=pkgs.begin();it!=pkgs.end();it++)
			{
				//添加TCP头即：$ channel size ;
				char	tcp_header[4];
				tcp_header[0] = '$';
				tcp_header[1] = m_session->RTP_channel;
				tcp_header[2] = ((*it).size >> 8);
				tcp_header[3] = ((*it).size&0xff);
				sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);
				if (sendRet<=0)
				{
					iRet = -1;
					break;
				}
				sendRet = socketSend(m_RTSP_socket_id, (*it).data, (*it).size);
				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
				m_rtp_pkg_counts++;
				m_rtp_data_size += (*it).size;
				delete[](*it).data;
				//LOG_INFO(configData::getInstance().get_loggerId(),  "time");
			}
			pkgs.clear();
			
		}
		else if (m_session->transport_type==RTSP_OVER_UDP)
		{
			/*LOG_WARN(configData::getInstance().get_loggerId(), "not send udp");*/
			//break;
			//RTP
			iRet = generate_sendable_frame(pkgs, h264_tcp_data_size+ h264_tcp_header_size);
			if (iRet != 0)
			{
				//Just send bye;
				memset(&remote_addr, 0, sizeof remote_addr);
				remote_addr.sin_family = AF_INET;
				remote_addr.sin_port = htons(m_session->RTCP_c_port);
				memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
				RTCP_process Rtcp_process;
				DataPacket bye_packet;
				bye_packet.data = 0;
				Rtcp_process.generate_Bye_Buf(bye_packet, m_rtcp_ssrc);
				sendRet = socketSendto(m_rtcp_socket, bye_packet.data, bye_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
				delete[]bye_packet.data;
				bye_packet.data = 0;
				break;
			}
			std::list<DataPacket>::iterator it;
			for (it = pkgs.begin(); it != pkgs.end(); it++)
			{
				memset(&remote_addr, 0, sizeof remote_addr);
				remote_addr.sin_family = AF_INET;
				remote_addr.sin_port = htons(m_session->RTP_c_port);
				memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
				sendRet = socketSendto(m_rtp_socket, (*it).data, (*it).size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
				m_rtp_pkg_counts++;
				m_rtp_data_size += (*it).size;
				delete[](*it).data;
			}
			pkgs.clear();
			//RTCP
			//SR
			//SDES
		}
		else
		{
			iRet = -1;
			break;
		}

		//RTCP
		timeval time_now;
		gettimeofday(&time_now, 0);
		if (time_now.tv_sec>m_rtcp_time.tv_sec+30)
		{
			m_rtcp_time = time_now;
			//SR
			DataPacket SR_packet,SDES_packet;
			iRet = RTCP_process::generate_RTCP_SR_buf(SR_packet, m_rtcp_ssrc, m_rtp_pkg_counts, m_rtp_data_size);
			if (iRet != 0)
			{
				break;
			}
			//SDES
			iRet = RTCP_process::generate_SDES_Buf(SDES_packet, m_rtcp_ssrc);
			if (iRet!=0)
			{
				break;
			}
			if (m_session->transport_type==RTSP_OVER_TCP)
			{
				char	tcp_header[4];
				int total_size = SR_packet.size + SDES_packet.size;
				tcp_header[0] = '$';
				tcp_header[1] = m_session->RTCP_channel;
				tcp_header[2] = (total_size >> 8);
				tcp_header[3] = (total_size & 0xff);
				sendRet = socketSend(m_RTSP_socket_id, tcp_header, 4);

				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
				sendRet = socketSend(m_RTSP_socket_id, SR_packet.data,SR_packet.size);
				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
				sendRet = socketSend(m_RTSP_socket_id, SDES_packet.data, SDES_packet.size);
				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
			}
			else
			{
				/*LOG_WARN(configData::getInstance().get_loggerId(), "not send udp");
				break;*/
				memset(&remote_addr, 0, sizeof remote_addr);
				remote_addr.sin_family = AF_INET;
				remote_addr.sin_port = htons(m_session->RTCP_c_port);
				memcpy(&remote_addr.sin_addr, &m_sock_addr, sizeof m_sock_addr);
				sendRet = socketSendto(m_rtcp_socket, SR_packet.data,SR_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
				sendRet = socketSendto(m_rtcp_socket, SDES_packet.data, SDES_packet.size, 0, (const sockaddr*)&remote_addr, remote_addr_len);
				if (sendRet <= 0)
				{
					iRet = -1;
					break;
				}
			}

			delete[]SR_packet.data;
			SR_packet.data = 0;
			delete[]SDES_packet.data;
			SDES_packet.data = 0;
		}
	}while (0);
	return iRet;
}

int H264VideoFileStreamSource::generate_sendable_frame(std::list<DataPacket>& frames, int frame_size)
{
	//没有CSRC，一个RTP header12个字节;
	const int RTP_HEADER_SIZE = 12;
	const int FU_header_size = 1;
	const int F_NRI_type_size = 1;
	//264 本身包含 F NRI ＴＹＰＥ；
	int payload_size = frame_size - RTP_HEADER_SIZE - F_NRI_type_size;
	int iRet = 0;

	bool read_again = false;
	char rtp_header_data[12];

	RTP_header  rtp_header;

	do
	{

		if (m_frame_size <= 0)
		{
			iRet = -1;
			break;
		}


		//如果是SPS或PPS，继续读取下一帧;
		do
		{
			read_again = false;
			iRet = get_frame();
			if (iRet != 0)
			{
				break;
			}

			//帧类型
			if (!m_frame_data || m_frame_size <= 0)
			{
				iRet = -1;
				break;
			}
			h264_nal_type	nal_type = (h264_nal_type)(m_frame_data[0] & 0x1f);
			char *stap_a_data = 0;
			int stap_a_len;

			DataPacket sps_pps_pkg;
			//只分为：单一包(数据较小)和分片包(数据较大)和单一时间组合包（SPS PPS）;
			switch (nal_type)
			{
			/*case h264_nal_sps:
				if (m_sps)
				{
					delete[]m_sps;
					m_sps = 0;
				}
				m_sps = new char[m_frame_size];
				memcpy(m_sps, m_frame_data, m_frame_size);
				m_sps_len = m_frame_size;
				read_again = true;
				break;
			case h264_nal_pps:
				if (m_pps)
				{
					delete[]m_pps;
					m_pps = 0;
				}
				m_pps = new char[m_frame_size];
				memcpy(m_pps, m_frame_data, m_frame_size);
				m_pps_len = m_frame_size;
				read_again = true;
				break;*/
			case h264_nal_slice_idr:

				//添加SPS，PPS这个STAP_A包;RFC3984 5.7.1;
				stap_a_len = 1 + 2 + 2 + m_sps_len + m_pps_len;//
				stap_a_data = new char[1+2+2+m_sps_len+m_pps_len];
				int stap_cur ;
				stap_cur = 0;
				//STAP_A hdr即 forbiddenzero nri type
				unsigned char forbidden_zero_bit, nri, type;
				forbidden_zero_bit = 0;
				//提取nri;
				nri = ((m_sps[0] & 0x60) >> 5);
				type = NAL_TYPE_STAP_A;
				stap_a_data[stap_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
				stap_cur++;
				//sps size;
				unsigned short nal_size;
				nal_size = m_sps_len;
				nal_size = htons(nal_size);
				memcpy(stap_a_data + stap_cur, &nal_size, 2);
				stap_cur += 2;
				//sps data;
				memcpy(stap_a_data + stap_cur, m_sps, m_sps_len);
				stap_cur += m_sps_len;
				//pps size;
				nal_size = m_pps_len;
				nal_size = htons(nal_size);
				memcpy(stap_a_data + stap_cur, &nal_size, 2);
				stap_cur += 2;
				//pps data;
				memcpy(stap_a_data + stap_cur, m_pps, m_pps_len);
				stap_cur += m_pps_len;
				sps_pps_pkg.size = RTP_HEADER_SIZE + stap_a_len;
				sps_pps_pkg.data = new char[sps_pps_pkg.size];

				fillRtpHeader2Buf(rtp_header, rtp_header_data);
				memcpy(sps_pps_pkg.data, rtp_header_data, RTP_HEADER_SIZE);
				memcpy(sps_pps_pkg.data + RTP_HEADER_SIZE, stap_a_data, stap_a_len);
				delete[]stap_a_data;
				frames.push_back(sps_pps_pkg);
				//no break;
			case h264_nal_sps:
			case h264_nal_pps:
			case h264_nal_slice:
			case h264_nal_slice_dpa:
			case h264_nal_slice_dpb:
			case h264_nal_slice_dpc:
			case h264_nal_sei:
			case h264_nal_aud:
			case h264_nal_filter:
				//单一包(数据较小)和分片包(数据较大);
				if (m_frame_size<=payload_size)
				{
					//单一包和STAP包：F NRI TYPE H264的nal自带，无需添加;
					DataPacket single_pkg;

					single_pkg.size = m_frame_size+RTP_HEADER_SIZE;
					single_pkg.data = new char[single_pkg.size];

					fillRtpHeader2Buf(rtp_header, rtp_header_data);
					memcpy(single_pkg.data, rtp_header_data, RTP_HEADER_SIZE);
					memcpy(single_pkg.data + RTP_HEADER_SIZE, m_frame_data, m_frame_size);
					frames.push_back(single_pkg);
					//printf("single pkg\n");
				}
				//分片包;FU_A;RFC 3984 page29;
				else
				{
					//FU_A :F NRI TYPE的TYPE为24，真正的TYPE是 SER TYPE的TYPE;
					//break;
					unsigned char FU_S, FU_E, FU_R, FU_Type;
					unsigned char forbidden_zero_bit, nri, type;
					forbidden_zero_bit = 0;
					//提取nri;
					nri = ((m_frame_data[0]&0x60)>>5);
					type = NAL_TYPE_FU_A;
					FU_Type = nal_type;
					//FU header size 1;
					payload_size -= FU_header_size;
					int num_frames = m_frame_size / payload_size;
					int cur_frame;
					if (num_frames*payload_size<m_frame_size)
					{
						num_frames++;
					}
					cur_frame = 0;
					DataPacket	fu_a_pkg;
					//已经读取的数据量;
					int cur_frame_cur;
					//pkg保存进度;
					int pkg_cur;
					//第一帧;1 0 0
					FU_S = 1;
					FU_E = 0;
					FU_R = 0;
					//不需要 F NRI TYPE;
					cur_frame_cur = 1;
					pkg_cur = 0;
					fu_a_pkg.size = payload_size + FU_header_size + F_NRI_type_size + RTP_HEADER_SIZE;
					fu_a_pkg.size = frame_size;
					fu_a_pkg.data = new char[fu_a_pkg.size];

					fillRtpHeader2Buf(rtp_header, rtp_header_data);
					memcpy(fu_a_pkg.data+pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
					pkg_cur += RTP_HEADER_SIZE;
					fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
					pkg_cur += F_NRI_type_size;
					fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E<<6) | (FU_R<<5) | (FU_Type));
					pkg_cur += FU_header_size;
					memcpy(fu_a_pkg.data + pkg_cur, m_frame_data + cur_frame_cur, payload_size);
					pkg_cur += payload_size;
					cur_frame_cur += payload_size;
					frames.push_back(fu_a_pkg);
					cur_frame++;
					//中间帧:0 0 0
					FU_S = 0;
					FU_E = 0;
					FU_R = 0;

					while (cur_frame+1<num_frames)
					{
						pkg_cur = 0;
						fu_a_pkg.size = payload_size + F_NRI_type_size + FU_header_size + RTP_HEADER_SIZE;
						fu_a_pkg.size = frame_size;
						fu_a_pkg.data = new char[fu_a_pkg.size];

						fillRtpHeader2Buf(rtp_header, rtp_header_data);
						memcpy(fu_a_pkg.data + pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
						pkg_cur += RTP_HEADER_SIZE;
						fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
						pkg_cur += F_NRI_type_size;
						fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E << 6) | (FU_R << 5) | (FU_Type));
						pkg_cur += FU_header_size;
						memcpy(fu_a_pkg.data + pkg_cur, m_frame_data + cur_frame_cur, payload_size);
						pkg_cur += payload_size;
						cur_frame_cur += payload_size;
						frames.push_back(fu_a_pkg);
						cur_frame++;
					}
					//最后帧:0 1 0
					FU_S = 0;
					FU_E = 1;
					FU_R = 0;

					pkg_cur = 0;
					//剩下的数据大小;
					int last_frame_size;
					last_frame_size = m_frame_size - cur_frame_cur;
					if (last_frame_size>0)
					{
						fu_a_pkg.size = m_frame_size - cur_frame_cur + F_NRI_type_size + FU_header_size + RTP_HEADER_SIZE;
						fu_a_pkg.data = new char[fu_a_pkg.size];

						fillRtpHeader2Buf(rtp_header, rtp_header_data);
						memcpy(fu_a_pkg.data + pkg_cur, rtp_header_data, RTP_HEADER_SIZE);
						pkg_cur += RTP_HEADER_SIZE;
						fu_a_pkg.data[pkg_cur] = ((forbidden_zero_bit << 7) | (nri << 5) | (type));
						pkg_cur += F_NRI_type_size;
						fu_a_pkg.data[pkg_cur] = ((FU_S << 7) | (FU_E << 6) | (FU_R << 5) | (FU_Type));
						pkg_cur += FU_header_size;
						memcpy(fu_a_pkg.data + pkg_cur, m_frame_data + cur_frame_cur, last_frame_size);
						pkg_cur += last_frame_size;
						cur_frame_cur += last_frame_size;
						frames.push_back(fu_a_pkg);
					}
				}
				break;
			default:
				iRet = -1;
				break;
			}
		} while (read_again);

	} while (0);

	return iRet;
}

unsigned short H264VideoFileStreamSource::incrementSeq()
{
	if (m_seq++ == 0xffff)
	{
		m_seq = 0;
		m_seq_num++;
	}
	return m_seq;
}

void H264VideoFileStreamSource::fillRtpHeader2Buf(RTP_header & rtp_header, char * buf)
{
	timeval time_now;
	rtp_header.version = 2;
	rtp_header.padding = 0;
	rtp_header.extension = 0;
	rtp_header.cc = 0;
	rtp_header.marker = 0;
	rtp_header.payload_type = RTP_H264_Payload_type;
	gettimeofday(&time_now, 0);
	rtp_header.ssrc = htonl(m_session->ssrc);
	rtp_header.seq = htons(incrementSeq());
	//time_now.tv_sec %= 0x10000000;
	rtp_header.ts = time_now.tv_sec*RTP_video_freq + time_now.tv_usec * 9 / 100 ;
	//32bit转换为网络字节序;
	rtp_header.ts = htonl(rtp_header.ts);
	int header_size = sizeof rtp_header;
	//生成RTP头;
	int rtp_header_cur = 0;
	buf[rtp_header_cur++] = ((rtp_header.version << 6) | (rtp_header.padding << 5) |
		(rtp_header.extension << 4) | (rtp_header.cc));
	buf[rtp_header_cur++] = ((rtp_header.marker << 7) | (rtp_header.payload_type));
	memcpy(buf + rtp_header_cur, &rtp_header.seq, 2);
	rtp_header_cur += 2;
	memcpy(buf + rtp_header_cur, &rtp_header.ts, 4);
	rtp_header_cur += 4;
	memcpy(buf + rtp_header_cur, &rtp_header.ssrc, 4);
	rtp_header_cur += 4;
}

int H264VideoFileStreamSource::handTask(taskScheduleObj *task_obj)
{
	int iRet=0;
	do
	{
		if (0==task_obj)
		{
			iRet = -1;
			break;
		}
		task_schedule_type task_type = task_obj->getType();
		taskStreamStop *task_stream_stop = 0;
		taskStreamPause *task_stream_pause = nullptr;
		taskRtcpMessage *task_RTCP_message = 0;
		taskSendRtspResbData *taskSendRtspData = nullptr;
		switch (task_type)
		{
		case schedule_start_rtsp_file:
			iRet = startStream(task_obj);
			break;
		case schedule_start_rtsp_living:
			iRet = -1;
			LOGW("not processed");
			break;
		case schedule_stop_rtsp_by_socket:
			task_stream_stop = static_cast<taskStreamStop*>(task_obj);
			if (task_stream_stop->getSocketId()==m_RTSP_socket_id)
			{
				
				iRet = stopStream(task_obj);
				break;
			}else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_stop_rtsp_by_name:
			task_stream_stop = static_cast<taskStreamStop*>(task_obj);
			if (task_stream_stop->getSessionName() == m_session->session_id)
			{
				iRet = stopStream(task_obj);
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_pause_rtsp_file:
			task_stream_pause = static_cast<taskStreamPause*>(task_obj);
			if (task_stream_pause->getSessionName()==m_session->session_id)
			{
				m_hasCalledDelete = true;
				m_send_end = true;
				if (m_thread_send.joinable())
				{
					m_thread_send.join();
				}
				m_send_end = false;
				m_hasCalledDelete = false;

				iRet = 0;
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_rtp_data:
			iRet = -1;
			LOGW("not processed");
			break;
		case schedule_rtcp_data:
			task_RTCP_message = static_cast<taskRtcpMessage*>(task_obj);
			RTCP_process *rtcp_process;
			rtcp_process = task_RTCP_message->get_RTCP_message();
			if (!rtcp_process)
			{
				iRet = -1;
				break;
			}
			m_rtcp_ssrc = rtcp_process->RTCP_SSRC();
			if (rtcp_process->RTP_ssrc(0)==m_session->ssrc)
			{
				iRet = 0;
				break;
			}
			else
			{
				iRet = -1;
				break;
			}
			break;
		case schedule_send_rtsp_data:
			taskSendRtspData = static_cast<taskSendRtspResbData*>(task_obj);
			if (taskSendRtspData->getSessionId()!=m_session->session_id)
			{
				iRet = -1;
				break;
			}
			m_mutexRtspSend.lock();
			m_vectorRtspSendData.push_back(taskSendRtspData->getDataSend());
			m_mutexRtspSend.unlock();
			break;
		default:
			iRet = -1;
			break;
		}
	} while (0);
	return iRet;
}

void H264VideoFileStreamSource::initStreamFile()
{
	m_tmp_buf_in=(char*)malloc(m_tmp_size);
	m_frame_data=(char*)malloc(m_frame_container_size);
	if (get_sps_pps())
	{
		anasys_sps_pps();
	}
}

bool H264VideoFileStreamSource::get_sps_pps()
{
	bool bok=true;
	do
	{
		bok=(get_frame()==0);
		if (!bok)
		{
			break;
		}
		//sps;
		if (m_frame_size <= 0 || ((m_frame_data[0]&0x1f)!=h264_nal_sps))
		{
			bok = false;
			break;
		}
		m_sps_len = m_frame_size;
		m_sps = new char[m_sps_len];
		memcpy(m_sps, m_frame_data, m_sps_len);
		//pps;
		bok = (get_frame() == 0);
		if (!bok)
		{
			break;
		}
		if (m_frame_size<=0||((m_frame_data[0]&0x1f)!=h264_nal_pps))
		{
			bok = false;
			break;
		}
		m_pps_len = m_frame_size;
		m_pps = new char[m_pps_len];
		memcpy(m_pps, m_frame_data, m_pps_len);
	} while (0);
	return	bok;
}

bool H264VideoFileStreamSource::anasys_sps_pps()
{
	bool bok=true;
	char *tmp_sps = 0;
	int web_sps_len = 0;
	CBitReader	bit_reader;
	do
	{
		tmp_sps = new char[m_sps_len];
		memcpy(tmp_sps, m_sps, m_sps_len);
		web_sps_len = m_sps_len;
		emulation_prevention((unsigned char*)tmp_sps, web_sps_len);
		bit_reader.SetBitReader((unsigned char*)tmp_sps, web_sps_len);
		bit_reader.ReadBits(8);
		int frame_crop_left_offset = 0;
		int frame_crop_right_offset = 0;
		int frame_crop_top_offset = 0;
		int frame_crop_bottom_offset = 0;

		int profile_idc = bit_reader.ReadBits(8);
		int constraint_set0_flag = bit_reader.ReadBit();
		int constraint_set1_flag = bit_reader.ReadBit();
		int constraint_set2_flag = bit_reader.ReadBit();
		int constraint_set3_flag = bit_reader.ReadBit();
		int constraint_set4_flag = bit_reader.ReadBit();
		int constraint_set5_flag = bit_reader.ReadBit();
		int reserved_zero_2bits = bit_reader.ReadBits(2);
		int level_idc = bit_reader.ReadBits(8);
		int seq_parameter_set_id = bit_reader.ReadExponentialGolombCode();


		if (profile_idc == 100 || profile_idc == 110 ||
			profile_idc == 122 || profile_idc == 244 ||
			profile_idc == 44 || profile_idc == 83 ||
			profile_idc == 86 || profile_idc == 118)
		{
			int chroma_format_idc = bit_reader.ReadExponentialGolombCode();

			if (chroma_format_idc == 3)
			{
				int residual_colour_transform_flag = bit_reader.ReadBit();
			}
			int bit_depth_luma_minus8 = bit_reader.ReadExponentialGolombCode();
			int bit_depth_chroma_minus8 = bit_reader.ReadExponentialGolombCode();
			int qpprime_y_zero_transform_bypass_flag = bit_reader.ReadBit();
			int seq_scaling_matrix_present_flag = bit_reader.ReadBit();

			if (seq_scaling_matrix_present_flag)
			{
				int i = 0;
				for (i = 0; i < 8; i++)
				{
					int seq_scaling_list_present_flag = bit_reader.ReadBit();
					if (seq_scaling_list_present_flag)
					{
						int sizeOfScalingList = (i < 6) ? 16 : 64;
						int lastScale = 8;
						int nextScale = 8;
						int j = 0;
						for (j = 0; j < sizeOfScalingList; j++)
						{
							if (nextScale != 0)
							{
								int delta_scale = bit_reader.ReadSE();
								nextScale = (lastScale + delta_scale + 256) % 256;
							}
							lastScale = (nextScale == 0) ? lastScale : nextScale;
						}
					}
				}
			}
		}

		int log2_max_frame_num_minus4 = bit_reader.ReadExponentialGolombCode();
		int pic_order_cnt_type = bit_reader.ReadExponentialGolombCode();
		if (pic_order_cnt_type == 0)
		{
			int log2_max_pic_order_cnt_lsb_minus4 = bit_reader.ReadExponentialGolombCode();
		}
		else if (pic_order_cnt_type == 1)
		{
			int delta_pic_order_always_zero_flag = bit_reader.ReadBit();
			int offset_for_non_ref_pic = bit_reader.ReadSE();
			int offset_for_top_to_bottom_field = bit_reader.ReadSE();
			int num_ref_frames_in_pic_order_cnt_cycle = bit_reader.ReadExponentialGolombCode();
			int i;
			for (i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++)
			{
				bit_reader.ReadSE();
				//sps->offset_for_ref_frame[ i ] = ReadSE();
			}
		}
		int max_num_ref_frames = bit_reader.ReadExponentialGolombCode();
		int gaps_in_frame_num_value_allowed_flag = bit_reader.ReadBit();
		int pic_width_in_mbs_minus1 = bit_reader.ReadExponentialGolombCode();
		int pic_height_in_map_units_minus1 = bit_reader.ReadExponentialGolombCode();
		int frame_mbs_only_flag = bit_reader.ReadBit();
		if (!frame_mbs_only_flag)
		{
			int mb_adaptive_frame_field_flag = bit_reader.ReadBit();
		}
		int direct_8x8_inference_flag = bit_reader.ReadBit();
		int frame_cropping_flag = bit_reader.ReadBit();
		if (frame_cropping_flag)
		{
			frame_crop_left_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_right_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_top_offset = bit_reader.ReadExponentialGolombCode();
			frame_crop_bottom_offset = bit_reader.ReadExponentialGolombCode();
		}
		int Width = ((pic_width_in_mbs_minus1 + 1) * 16) - frame_crop_bottom_offset * 2 - frame_crop_top_offset * 2;
		int Height = ((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 + 1) * 16) - (frame_crop_right_offset * 2) - (frame_crop_left_offset * 2);
		m_video_width = Width;
		m_video_height = Height;
		int vui_parameters_present_flag = bit_reader.ReadBit();

		if (vui_parameters_present_flag)
		{
			int aspect_ratio_info_present_flag = bit_reader.ReadBit();
			if (aspect_ratio_info_present_flag)
			{
				int aspect_ratio_idc = bit_reader.ReadBits(8);
				if (aspect_ratio_idc == 255)
				{
					int sar_width = bit_reader.ReadBits(16);
					int sar_height = bit_reader.ReadBits(16);
				}
			}
			int overscan_info_present_flag = bit_reader.ReadBit();
			if (overscan_info_present_flag)
				int overscan_appropriate_flagu = bit_reader.ReadBit();
			int video_signal_type_present_flag = bit_reader.ReadBit();
			if (video_signal_type_present_flag)
			{
				int video_format = bit_reader.ReadBits(3);
				int video_full_range_flag = bit_reader.ReadBit();
				int colour_description_present_flag = bit_reader.ReadBit();
				if (colour_description_present_flag)
				{
					int colour_primaries = bit_reader.ReadBits(8);
					int transfer_characteristics = bit_reader.ReadBits(8);
					int matrix_coefficients = bit_reader.ReadBits(8);
				}
			}
			int chroma_loc_info_present_flag = bit_reader.ReadBit();
			if (chroma_loc_info_present_flag)
			{
				int chroma_sample_loc_type_top_field = bit_reader.ReadExponentialGolombCode();
				int chroma_sample_loc_type_bottom_field = bit_reader.ReadExponentialGolombCode();
			}
			int timing_info_present_flag = bit_reader.ReadBit();
			if (timing_info_present_flag)
			{
				int num_units_in_tick = bit_reader.ReadBits(32);
				int time_scale = bit_reader.ReadBits(32);
				m_fps = time_scale / (2 * num_units_in_tick);
			}
		}
	} while (0);
	if (tmp_sps)
	{
		delete[]tmp_sps;
		tmp_sps = 0;
	}

	return bok;
}

int H264VideoFileStreamSource::get_frame()
{
	int read_pos=0;
	int iRet=0;
	int one=1;
	int nal_start=0;
	int nal_end=0;
	bool	get_nal=false;
	do
	{
		//读取nal开头;
		do
		{
			if (read_pos>=m_tmp_size-1)
			{
				m_tmp_size*=2;
				m_tmp_buf_in=(char*)realloc(m_tmp_buf_in,m_tmp_size);
			}
			iRet=m_file_reader->readFile(m_tmp_buf_in+read_pos++,one);
			if (iRet<0)
			{
				break;
			}
			if (read_pos-nal_start<5)
			{
				continue;
			}
			if (m_tmp_buf_in[nal_start]==0x00&&
				m_tmp_buf_in[nal_start+1]==0x00)
			{
				if (m_tmp_buf_in[nal_start+2]==0x01)
				{
					nal_start+=3;
					get_nal=true;
					break;
				}else if (m_tmp_buf_in[nal_start+2]==0x00)
				{
					if (m_tmp_buf_in[nal_start+3]==0x01)
					{
						nal_start+=4;
						get_nal=true;
						break;
					}
				}else
				{
					nal_start+=2;
				}
			}
			nal_start++;
		} while (true);
		if (!get_nal)
		{
			iRet=-1;
			break;
		}
		//读取nal结尾;
		get_nal=false;
		nal_end=nal_start;
		do
		{
			if (read_pos>=m_tmp_size-1)
			{
				m_tmp_size*=2;
				m_tmp_buf_in=(char*)realloc(m_tmp_buf_in,m_tmp_size);
			}
			iRet=m_file_reader->readFile(m_tmp_buf_in+read_pos++,one);
			if (iRet<0)
			{
				break;
			}
			if (read_pos-nal_end<4)
			{
				continue;
			}
			if (m_tmp_buf_in[nal_end]==0x00&&
				m_tmp_buf_in[nal_end+1]==0x00)
			{
				if (m_tmp_buf_in[nal_end+2]==0x01)
				{
					get_nal=true;
					m_file_reader->backword(4);
					break;
				}
				else if (m_tmp_buf_in[nal_end+2]==0x00)
				{
					if (m_tmp_buf_in[nal_end+3]==0x01)
					{
						get_nal=true;
						m_file_reader->backword(5);
						break;
					}
				}else
				{
					nal_end+=2;
				}
			}
			nal_end++;
		} while (true);
		if (!get_nal)
		{
			iRet=-1;
			break;
		}
		//取nal;
		m_frame_size=nal_end-nal_start;
		if (m_frame_container_size<m_frame_size)
		{
			m_frame_container_size=m_frame_size*2;
			m_frame_data=(char*)realloc(m_frame_data,m_frame_container_size);
		}
		memcpy(m_frame_data,m_tmp_buf_in+nal_start,m_frame_size);
	} while (0);
	return iRet;
}
