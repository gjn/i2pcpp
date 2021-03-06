#include "SearchManager.h"

#include "../i2np/DatabaseLookup.h"

#include "../RouterContext.h"

#include "util/Base64.h"

namespace i2pcpp {
	namespace DHT {
		SearchManager::SearchManager(boost::asio::io_service &ios, RouterContext &ctx) :
			m_ios(ios),
			m_ctx(ctx),
			m_log(boost::log::keywords::channel = "SM") {}

		SearchManager::~SearchManager() {}

		boost::signals2::connection SearchManager::registerSuccess(SuccessSignal::slot_type const &sh)
		{
			return m_successSignal.connect(sh);
		}

		boost::signals2::connection SearchManager::registerFailure(FailureSignal::slot_type const &fh)
		{
			return m_failureSignal.connect(fh);
		}

		void SearchManager::createSearch(KademliaKey const &k, RouterHash const &start)
		{
			std::lock_guard<std::mutex> lock(m_searchesMutex);

			if(m_searches.get<0>().count(k)) return;

			SearchState ss;
			ss.goal = k;
			ss.current = start;

			m_searches.insert(ss);

			m_timers[k] = std::make_shared<boost::asio::deadline_timer>(m_ios, boost::posix_time::time_duration(0, 2, 0));
			m_timers[k]->async_wait(boost::bind(&SearchManager::timeout, this, boost::asio::placeholders::error, k));

			I2P_LOG(m_log, debug) << "created SearchState for " << Base64::encode(ByteArray(k.cbegin(), k.cend())) << " starting with " << start;

			m_ctx.getOutMsgDisp().getTransport()->connect(m_ctx.getDatabase().getRouterInfo(start));
		}

		void SearchManager::timeout(const boost::system::error_code& e, KademliaKey const k)
		{
			if(!e) {
				I2P_LOG(m_log, debug) << "timeout for " << Base64::encode(ByteArray(k.cbegin(), k.cend()));

				std::lock_guard<std::mutex> lock(m_searchesMutex);
				cancel(k);
			}
		}

		void SearchManager::cancel(KademliaKey const &k)
		{
			SearchStateByGoal::iterator itr = m_searches.get<0>().find(k);

			if(itr != m_searches.get<0>().end()) {
				m_ios.post(boost::bind(boost::ref(m_failureSignal), itr->goal));

				m_searches.get<0>().erase(itr);
				m_timers.erase(k);
			}
		}

		void SearchManager::connected(RouterHash const rh)
		{
			I2P_LOG_SCOPED_RH(m_log, rh);
			I2P_LOG(m_log, debug) << "connection established";

			std::lock_guard<std::mutex> lock(m_searchesMutex);

			if(m_searches.get<1>().count(rh)) {
				I2P_LOG(m_log, debug) << "found RouterHash in pending search table";

				SearchStateByCurrent::iterator itr = m_searches.get<1>().find(rh);
				const SearchState& ss = *itr;

				if(ss.state == SearchState::CONNECTING) {
					I2P_LOG(m_log, debug) << "found good SearchState, sending DatabaseLookup";

					m_searches.get<1>().modify(itr, ModifyState(SearchState::LOOKUP_SENT));

					I2NP::MessagePtr dbl(new I2NP::DatabaseLookup(ss.goal, m_ctx.getIdentity().getHash(), 0, ss.excluded));
					m_ctx.getOutMsgDisp().getTransport()->send(rh, dbl->toBytes());
				}
			}
		}

		void SearchManager::connectionFailure(RouterHash const rh)
		{
			std::lock_guard<std::mutex> lock(m_searchesMutex);

			if(m_searches.get<1>().count(rh)) {
				I2P_LOG_SCOPED_RH(m_log, rh);

				SearchStateByCurrent::iterator itr = m_searches.get<1>().find(rh);
				const SearchState& ss = *itr;

				m_searches.get<1>().modify(itr, InsertTried(rh));

				RouterHash front;
				if(ss.alternates.size()) {
					front = ss.alternates.front();
					while(!m_ctx.getDatabase().routerExists(front)) {
						I2P_LOG(m_log, debug) << "could not try alternate " << front << " because it doesn't exist";

						m_searches.get<1>().modify(itr, PopAlternates());

						if(ss.alternates.size())
							front = ss.alternates.front();
						else {
							I2P_LOG(m_log, debug) << "no more alternates left, search failed";

							cancel(ss.goal);

							return;
						}
					}

					while(ss.tried.count(front)) {
						m_searches.get<1>().modify(itr, PopAlternates());

						if(ss.alternates.size())
							front = ss.alternates.front();
						else {
							I2P_LOG(m_log, debug) << "no more alternates left, search failed";

							cancel(ss.goal);

							return;
						}
					}

					I2P_LOG(m_log, debug) << "connecting to alternate " << front;

					m_searches.get<1>().modify(itr, ModifyState(front, rh));

					m_ctx.getOutMsgDisp().getTransport()->connect(m_ctx.getDatabase().getRouterInfo(front));
				} else {
					I2P_LOG(m_log, debug) << "connection failed and there are no more alternates, search failed";

					cancel(ss.goal);

					return;
				}
			}
		}

		void SearchManager::searchReply(RouterHash const from, std::array<unsigned char, 32> const query, std::list<RouterHash> const hashes)
		{
			I2P_LOG_SCOPED_RH(m_log, from);
			I2P_LOG(m_log, debug) << "received search reply";

			std::lock_guard<std::mutex> lock(m_searchesMutex);

			if(m_searches.get<0>().count(query)) {
				I2P_LOG(m_log, debug) << "found RouterHash in pending search table";

				SearchStateByGoal::iterator itr = m_searches.get<0>().find(query);
				const SearchState& ss = *itr;

				if(ss.state == SearchState::LOOKUP_SENT) {
					m_searches.get<0>().modify(itr, InsertTried(from));

					for(auto h: hashes) {
						if(!ss.tried.count(h))
							m_searches.get<0>().modify(itr, PushAlternates(h));
					}

					RouterHash front;
					if(ss.alternates.size()) {
						front = ss.alternates.front();
						while(ss.tried.count(front)) {
							m_searches.get<0>().modify(itr, PopAlternates());

							if(ss.alternates.size())
								front = ss.alternates.front();
							else {
								I2P_LOG(m_log, debug) << "no more alternates left, search failed";

								cancel(ss.goal);

								return;
							}
						}

						if(!m_ctx.getDatabase().routerExists(front)) {
							I2P_LOG(m_log, debug) << "received unknown peer hash " << front << ", asking for its RouterInfo";

							m_searches.get<0>().modify(itr, ModifyState(front));

							I2NP::MessagePtr dbl(new I2NP::DatabaseLookup(front, m_ctx.getIdentity().getHash(), 0, ss.excluded));
							m_ctx.getOutMsgDisp().sendMessage(from, dbl);
						} else {
							I2P_LOG(m_log, debug) << "received known peer hash " << front << ", connecting";

							m_searches.get<0>().modify(itr, ModifyState(front, from));

							m_ctx.getOutMsgDisp().getTransport()->connect(m_ctx.getDatabase().getRouterInfo(front));
						}
					}
				}
			}
		}

		void SearchManager::databaseStore(RouterHash const from, std::array<unsigned char, 32> const k, bool isRouterInfo)
		{
			I2P_LOG_SCOPED_RH(m_log, from);
			I2P_LOG(m_log, debug) << "received DatabaseStore";

			std::lock_guard<std::mutex> lock(m_searchesMutex);

			if(m_searches.get<0>().count(k)) {
				I2P_LOG(m_log, debug) << "received DatabaseStore for our goal, terminating search";

				SearchStateByGoal::iterator itr = m_searches.get<0>().find(k);
				const SearchState& ss = *itr;

				if(isRouterInfo)
					m_ios.post(boost::bind(boost::ref(m_successSignal), ss.goal, m_ctx.getDatabase().getRouterInfo(k).getIdentity().getHash()));

				m_searches.get<0>().erase(itr);
				m_timers.erase(k);

				return;
			}

			if(isRouterInfo) {
				if(m_searches.get<1>().count(from)) {
					I2P_LOG(m_log, debug) << "found Routerhash in pending search table";

					SearchStateByCurrent::iterator itr = m_searches.get<1>().find(from);
					const SearchState& ss = *itr;

					if(ss.next == k) {
						I2P_LOG(m_log, debug) << "stored hash is one we're waiting for, connecting";

						m_searches.get<1>().modify(itr, ModifyState(k, from));

						m_ctx.getOutMsgDisp().getTransport()->connect(m_ctx.getDatabase().getRouterInfo(k));
					}
				}
			}
		}
	}
}
