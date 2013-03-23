#include "AcknowledgementScheduler.h"

#include <boost/bind.hpp>

#include "../UDPTransport.h"

#include "Packet.h"
#include "PacketBuilder.h"
#include "PeerState.h"

namespace i2pcpp {
	namespace SSU {
		AcknowledgementScheduler::AcknowledgementScheduler(UDPTransport &transport) :
			m_transport(transport)
		{
			AcknowledgementTimerPtr timer(new boost::asio::deadline_timer(m_transport.m_ios, boost::posix_time::time_duration(0, 0, 1)));

			timer->async_wait(boost::bind(&AcknowledgementScheduler::flushAckCallback, this, boost::asio::placeholders::error, timer));
		}

		void AcknowledgementScheduler::flushAckCallback(const boost::system::error_code& e, AcknowledgementTimerPtr &timer)
		{
			for(auto& peerPair: m_transport.m_peers) {
				AckList ackList;
				PeerStatePtr ps = peerPair.second;

				std::lock_guard<std::mutex> lock(ps->getMutex());

				for(auto itr = ps->begin(); itr != ps->end();) {
					ackList.push_front(std::make_pair(itr->first, itr->second->getAckStates()));
					if(itr->second->allFragmentsReceived()) {
						ps->delInboundMessageState(itr++);
						continue;
					}

					++itr;
				}

/*				if(ackList.size()) {
					std::forward_list<OutboundMessageState::FragmentPtr> emptyFragList;
					PacketPtr p = PacketBuilder::buildData(ps, false, emptyFragList, ackList);
					p->encrypt(ps->getCurrentSessionKey(), ps->getCurrentMacKey());
					m_transport.sendPacket(p);
				}*/
			}

			timer->expires_at(timer->expires_at() + boost::posix_time::time_duration(0, 0, 1));
			timer->async_wait(boost::bind(&AcknowledgementScheduler::flushAckCallback, this, boost::asio::placeholders::error, timer));
		}
	}
}
