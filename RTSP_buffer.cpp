#include "RTSP.h"
#include <iterator>
#include <algorithm>

LP_RTSP_buffer deepcopy_RTSP_buffer(LP_RTSP_buffer buff_in)
{
	if (!buff_in)
	{
		return 0;
	}
	LP_RTSP_buffer buff_out=0;
	buff_out=new RTSP_buffer;
	buff_out->sock_id=buff_in->sock_id;
	buff_out->port;

	strcpy(buff_out->in_buffer, buff_in->in_buffer);
	buff_out->in_size=buff_in->in_size;
#if USE_STD_STRING
	//buff_out->in_buffer = buff_in->in_buffer;
	buff_out->out_buffer = buff_in->out_buffer;
	buff_out->sdp_desc = buff_in->sdp_desc;
#else
	strcpy(buff_out->in_buffer, buff_in->in_buffer);
	strcpy(buff_out->out_buffer, buff_in->out_buffer);
	strcpy(buff_out->sdp_desc, buff_in->sdp_desc);
#endif // USE_STD_STRING
	buff_out->out_size=buff_in->out_size;

	buff_out->rtsp_seq=buff_in->rtsp_seq;
	buff_out->session_time = buff_in->session_time;


	strcpy(buff_out->source_name,buff_in->source_name);

	memcpy(&buff_out->sock_addr, &buff_in->sock_addr, sizeof buff_in->sock_addr);

	//std::copy(buff_in->session_list.begin(), buff_in->session_list.end(), std::back_inserter(buff_out->session_list));
	/*std::list<LP_RTSP_session>::iterator it;
	for (it=buff_in->session_list.begin();it!=buff_in->session_list.end();it++)
	{
		buff_out->session_list.push_back(deepcopy_RTSP_session(*it));
	}*/
	for (LP_RTSP_session i : buff_in->session_list)
	{
		buff_out->session_list.push_back(deepcopy_RTSP_session(i));
	}
	return buff_out;
}


//void delete_RTSP_session(LP_RTSP_session session)
//{
//	delete session;
//	session = 0;
//}

void free_RTSP_buffer(LP_RTSP_buffer buff_in)
{
	if (!buff_in)
	{
		return;
	}
	/*std::list<LP_RTSP_session>::iterator it;
	for (it = buff_in->session_list.begin(); it != buff_in->session_list.end(); it++)
	{
		delete *it;
	}*/
	//std::for_each(buff_in->session_list.begin(), buff_in->session_list.end(), delete_RTSP_session);
	for (auto i:buff_in->session_list)
	{
		delete i;
	}

	delete buff_in;
}

LP_RTSP_session deepcopy_RTSP_session(LP_RTSP_session session_in)
{
	LP_RTSP_session session_out=0;
	if (!session_in)
	{
		return	0;
	}
	session_out=new RTSP_session;
	session_out->cur_state=session_in->cur_state;
	session_out->session_id = session_in->session_id;
	session_out->ssrc=session_in->ssrc;
	session_out->RTP_channel=session_in->RTP_channel;
	session_out->RTCP_channel=session_in->RTCP_channel;
	session_out->RTP_s_port=session_in->RTP_s_port;
	session_out->RTCP_s_port=session_in->RTCP_s_port;
	session_out->RTP_c_port=session_in->RTP_c_port;
	session_out->RTCP_c_port=session_in->RTCP_c_port;
	session_out->transport_type=session_in->transport_type;
	session_out->unicast=session_in->unicast;
	strcpy(session_out->control_id,session_in->control_id);
	session_out->first_seq=session_in->first_seq;
	session_out->start_rtp_time=session_in->start_rtp_time;
	
	return session_out;
}