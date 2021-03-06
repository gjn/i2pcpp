#include "AcknowledgementScheduler.h"

#include <boost/bind.hpp>

#include "../UDPTransport.h"

#include "Packet.h"
#include "PacketBuilder.h"
#include "PeerState.h"
#include "InboundMessageState.h"
#include "OutboundMessageState.h"

namespace i2pcpp {
	namespace SSU {
		AcknowledgementScheduler::AcknowledgementScheduler(UDPTransport &transport) :
			m_transport(transport),
			m_timer(m_transport.m_ios, boost::posix_time::time_duration(0, 0, 1))
		{
			m_timer.async_wait(boost::bind(&AcknowledgementScheduler::flushAckCallback, this, boost::asio::placeholders::error));
		}

		void AcknowledgementScheduler::flushAckCallback(const boost::system::error_code& e)
		{
			for(auto& peerPair: m_transport.m_peers) {
				PeerStatePtr ps = peerPair.second;

				std::lock_guard<std::mutex> lock(ps->getMutex());

				CompleteAckList completeAckList;
				PartialAckList partialAckList;
				for(auto itr = ps->begin(); itr != ps->end();) {
					if(itr->second->allFragmentsReceived()) {
						completeAckList.push_back(itr->first);
						ps->delInboundMessageState(itr++);
					} else {
						partialAckList[itr->first] = itr->second->getFragmentsReceived();
						++itr;
					}
				}

				if(completeAckList.size() || partialAckList.size()) {
					std::vector<PacketBuilder::FragmentPtr> emptyFragList;
					PacketPtr p = PacketBuilder::buildData(ps->getEndpoint(), false, completeAckList, partialAckList, emptyFragList);
					p->encrypt(ps->getCurrentSessionKey(), ps->getCurrentMacKey());
					m_transport.sendPacket(p);
				}
			}

			m_timer.expires_at(m_timer.expires_at() + boost::posix_time::time_duration(0, 0, 1));
			m_timer.async_wait(boost::bind(&AcknowledgementScheduler::flushAckCallback, this, boost::asio::placeholders::error));
		}
	}
}
