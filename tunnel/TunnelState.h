#ifndef TUNNELSTATE_H
#define TUNNELSTATE_H

#include <list>
#include <vector>

#include "../datatypes/RouterHash.h"
#include "../datatypes/BuildRequestRecord.h"
#include "../datatypes/BuildResponseRecord.h"

namespace i2pcpp {
	class RouterContext;

	class TunnelState {
		public:
			enum Direction {
				INBOUND,
				OUTBOUND
			};

			enum State {
				REQUEST_SENT,
				OPERATIONAL,
				FAILURE
			};

			TunnelState(RouterContext &ctx, std::list<RouterHash> const &hops, Direction d);

			const std::list<BuildRequestRecord>& getRequest() const;
			uint32_t getTunnelId() const;
			const RouterHash& getTerminalHop() const;
			Direction getDirection() const;

			State getState() const;

			void parseResponseRecords(std::list<BuildResponseRecord> &records);

		private:
			void buildInboundRequest();
			void buildOutboundRequest();

			RouterContext& m_ctx;
			Direction m_direction;
			State m_state = State::REQUEST_SENT;

			std::vector<uint32_t> m_tunnelIds;
			std::vector<SessionKey> m_tunnelLayerKeys;
			std::vector<SessionKey> m_tunnelIVKeys;
			std::vector<SessionKey> m_replyKeys;
			std::vector<SessionKey> m_replyIVs;
			std::vector<uint32_t> m_nextMsgIds;

			std::list<RouterHash> m_hops;
			RouterHash m_terminalHop;
			uint32_t m_tunnelId;
			std::list<BuildRequestRecord> m_records;
	};

	typedef std::shared_ptr<TunnelState> TunnelStatePtr;
}

#endif