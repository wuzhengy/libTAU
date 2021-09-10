/*

Copyright (c) 2003-2021, Arvid Norberg
Copyright (c) 2003, Daniel Wallin
Copyright (c) 2004, Magnus Jonsson
Copyright (c) 2008, Andrew Resch
Copyright (c) 2015, Mikhail Titov
Copyright (c) 2015-2020, Steven Siloti
Copyright (c) 2016, Jonathan McDougall
Copyright (c) 2016-2021, Alden Torres
Copyright (c) 2016-2018, Pavel Pimenov
Copyright (c) 2016-2017, Andrei Kurushin
Copyright (c) 2017, Falcosc
Copyright (c) 2017, 2020, AllSeeingEyeTolledEweSew
Copyright (c) 2017, ximply
Copyright (c) 2018, Fernando Rodriguez
Copyright (c) 2018, d-komarov
Copyright (c) 2018, airium
Copyright (c) 2020, Viktor Elofsson
Copyright (c) 2020, Rosen Penev
Copyright (c) 2020, Paul-Louis Ageneau
Copyright (c) 2021, AdvenT
All rights reserved.

You may use, distribute and modify this code under the terms of the BSD license,
see LICENSE file.
*/

#include "libTAU/config.hpp"

#include <cstdarg> // for va_list
#include <ctime>
#include <algorithm>

#include <set>
#include <map>
#include <vector>
#include <cctype>
#include <memory>
#include <numeric>
#include <limits> // for numeric_limits
#include <cstdio> // for snprintf
#include <functional>

#include "libTAU/aux_/torrent.hpp"

#ifdef TORRENT_SSL_PEERS
#include "libTAU/aux_/ssl_stream.hpp"
#include "libTAU/aux_/ssl.hpp"
#endif // TORRENT_SSL_PEERS

#include "libTAU/announce_entry.hpp"
#include "libTAU/aux_/parse_url.hpp"
#include "libTAU/bencode.hpp"
#include "libTAU/hasher.hpp"
#include "libTAU/entry.hpp"
#include "libTAU/aux_/peer.hpp"
#include "libTAU/peer_id.hpp"
#include "libTAU/identify_client.hpp"
#include "libTAU/alert_types.hpp"
#include "libTAU/extensions.hpp"
#include "libTAU/aux_/session_interface.hpp"
#include "libTAU/aux_/instantiate_connection.hpp"
#include "libTAU/assert.hpp"
#include "libTAU/kademlia/dht_tracker.hpp"
#include "libTAU/aux_/http_connection.hpp"
#include "libTAU/aux_/random.hpp"
#include "libTAU/peer_class.hpp" // for peer_class
#include "libTAU/aux_/socket_io.hpp" // for read_*_endpoint
#include "libTAU/ip_filter.hpp"
#include "libTAU/performance_counters.hpp" // for counters
#include "libTAU/aux_/resolver_interface.hpp"
#include "libTAU/aux_/alloca.hpp"
#include "libTAU/aux_/alert_manager.hpp"
#include "libTAU/disk_interface.hpp"
#include "libTAU/aux_/ip_helpers.hpp" // for is_ip_address
#include "libTAU/download_priority.hpp"
#include "libTAU/hex.hpp" // to_hex
#include "libTAU/aux_/range.hpp"
#include "libTAU/aux_/merkle.hpp"
#include "libTAU/aux_/numeric_cast.hpp"
#include "libTAU/aux_/path.hpp"
#include "libTAU/aux_/generate_peer_id.hpp"
#include "libTAU/aux_/announce_entry.hpp"
#include "libTAU/aux_/ssl.hpp"

#ifndef TORRENT_DISABLE_LOGGING
#include "libTAU/aux_/session_impl.hpp" // for tracker_logger
#endif

#include "libTAU/aux_/torrent_impl.hpp"

using namespace std::placeholders;

namespace libTAU::aux {
namespace {

bool is_downloading_state(int const st)
{
	return false;
}
} // anonymous namespace

	torrent_hot_members::torrent_hot_members(aux::session_interface& ses
		, bool const session_paused)
		: m_ses(ses)
		, m_complete(0xffffff)
		, m_connections_initialized(false)
		, m_abort(false)
		, m_session_paused(session_paused)
#ifndef TORRENT_DISABLE_SHARE_MODE
#endif
		, m_have_all(false)
		, m_graceful_pause_mode(false)
		, m_max_connections(0xffffff)
	{}

	torrent::torrent( aux::session_interface& ses
		, bool const session_paused)
		: torrent_hot_members(ses, session_paused)
		, m_tracker_timer(ses.get_context())
		, m_inactivity_timer(ses.get_context())
		, m_stats_counters(ses.stats_counters())
		, m_sequence_number(-1)
		, m_peer_id(aux::generate_peer_id(settings()))
		, m_has_incoming(false)
		, m_files_checked(false)
		, m_announcing(false)
		, m_added(false)
		, m_auto_sequential(false)
		, m_seed_mode(false)
		, m_max_uploads((1 << 24) - 1)
		, m_num_uploads(0)
		, m_pending_active_change(false)
		, m_v2_piece_layers_validated(false)
		, m_connect_boost_counter(static_cast<std::uint8_t>(settings().get_int(settings_pack::torrent_connect_boost)))
		, m_incomplete(0xffffff)
		, m_ssl_torrent(false)
		, m_deleted(false)
		, m_current_gauge_state(static_cast<std::uint32_t>(no_gauge_state))
		, m_moving_storage(false)
		, m_inactive(false)
		, m_downloaded(0xffffff)
		, m_progress_ppm(0)
		, m_torrent_initialized(false)
		, m_outstanding_file_priority(false)
		, m_complete_sent(false)
	{
	}

	void torrent::load_merkle_trees(
		aux::vector<std::vector<sha256_hash>, file_index_t> trees_import
		, aux::vector<std::vector<bool>, file_index_t> mask)
	{
	}

	void torrent::inc_stats_counter(int c, int value)
	{ m_ses.stats_counters().inc_stats_counter(c, value); }

	int torrent::current_stats_state() const
	{
		return 0;
	}

	void torrent::update_gauge()
	{
		int const new_gauge_state = current_stats_state() - counters::num_checking_torrents;
		TORRENT_ASSERT(new_gauge_state >= 0);
		TORRENT_ASSERT(new_gauge_state <= no_gauge_state);

		if (new_gauge_state == int(m_current_gauge_state)) return;

		if (m_current_gauge_state != no_gauge_state)
			inc_stats_counter(m_current_gauge_state + counters::num_checking_torrents, -1);
		if (new_gauge_state != no_gauge_state)
			inc_stats_counter(new_gauge_state + counters::num_checking_torrents, 1);

		TORRENT_ASSERT(new_gauge_state >= 0);
		TORRENT_ASSERT(new_gauge_state <= no_gauge_state);
		m_current_gauge_state = static_cast<std::uint32_t>(new_gauge_state);
	}

	void torrent::leave_seed_mode(seed_mode_t const checking)
	{
	}

	void torrent::verified(piece_index_t const piece)
	{
		TORRENT_ASSERT(!m_verified.get_bit(piece));
		++m_num_verified;
		m_verified.set_bit(piece);
	}

	void torrent::start()
	{
	}

	void torrent::set_apply_ip_filter(bool b)
	{
		if (b == m_apply_ip_filter) return;
		if (b)
		{
			inc_stats_counter(counters::non_filter_torrents, -1);
		}
		else
		{
			inc_stats_counter(counters::non_filter_torrents);
		}

		set_need_save_resume();

		m_apply_ip_filter = b;
		ip_filter_updated();
		state_updated();
	}

	void torrent::set_ip_filter(std::shared_ptr<const ip_filter> ipf)
	{
		m_ip_filter = std::move(ipf);
		if (!m_apply_ip_filter) return;
		ip_filter_updated();
	}

#ifndef TORRENT_DISABLE_DHT
	bool torrent::should_announce_dht() const
	{
		return false;
	}

#endif

	torrent::~torrent()
	{
		// TODO: 3 assert there are no outstanding async operations on this
		// torrent

		// The invariant can't be maintained here, since the torrent
		// is being destructed, all weak references to it have been
		// reset, which means that all its peers already have an
		// invalidated torrent pointer (so it cannot be verified to be correct)

		// i.e. the invariant can only be maintained if all connections have
		// been closed by the time the torrent is destructed. And they are
		// supposed to be closed. So we can still do the invariant check.

		// however, the torrent object may be destructed from the main
		// thread when shutting down, if the disk cache has references to it.
		// this means that the invariant check that this is called from the
		// network thread cannot be maintained

		TORRENT_ASSERT(m_peer_class == peer_class_t{0});
	}

	void torrent::read_piece(piece_index_t const piece)
	{
	}

#ifndef TORRENT_DISABLE_SHARE_MODE
	void torrent::send_share_mode()
	{
	}
#endif // TORRENT_DISABLE_SHARE_MODE

	void torrent::send_upload_only()
	{
	}

#ifndef TORRENT_DISABLE_SHARE_MODE
	void torrent::set_share_mode(bool s)
	{
	}
#endif // TORRENT_DISABLE_SHARE_MODE

	void torrent::set_upload_mode(bool b)
	{
	}

	void torrent::handle_exception()
	{
	}

	storage_mode_t torrent::storage_mode() const
	{ return storage_mode_t(m_storage_mode); }

	void torrent::clear_peers()
	{
	}

	void torrent::set_sequential_range(piece_index_t first_piece, piece_index_t last_piece)
	{
	}

	void torrent::set_sequential_range(piece_index_t first_piece)
	{
	}

	void torrent::on_disk_write_complete(storage_error const& error
		, peer_request const& p) try
	{
	}
	catch (...) { handle_exception(); }

	std::string torrent::name() const
	{
		return "";
	}

#ifdef TORRENT_SSL_PEERS
	bool torrent::verify_peer_cert(bool const preverified, ssl::verify_context& ctx)
	{
		return false;
	}

	void torrent::init_ssl(string_view cert)
	{
	}
#endif // TORRENT_SSL_PEERS

	void torrent::construct_storage()
	{
	}

	// this may not be called from a constructor because of the call to
	// later time if it's a magnet link, once the metadata is downloaded
	void torrent::init()
	{
	}

	bool torrent::is_self_connection(peer_id const& pid) const
	{
		return m_outgoing_pids.count(pid) > 0;
	}

	void torrent::on_resume_data_checked(status_t const status
		, storage_error const& error) try
	{
	}
	catch (...) { handle_exception(); }

	void torrent::force_recheck()
	{
	}

	void torrent::on_force_recheck(status_t const status, storage_error const& error) try
	{
	}
	catch (...) { handle_exception(); }

	void torrent::start_checking() try
	{
	}
	catch (...) { handle_exception(); }

	// This is only used for checking of torrents. i.e. force-recheck or initial checking
	// of existing files
	void torrent::on_piece_hashed(aux::vector<sha256_hash> block_hashes
		, piece_index_t const piece, sha1_hash const& piece_hash
		, storage_error const& error) try
	{
	}
	catch (...) { handle_exception(); }

#if TORRENT_ABI_VERSION == 1
	void torrent::use_interface(std::string net_interfaces)
	{
		std::shared_ptr<settings_pack> p = std::make_shared<settings_pack>();
		p->set_str(settings_pack::outgoing_interfaces, std::move(net_interfaces));
		m_ses.apply_settings_pack(p);
	}
#endif

	void torrent::on_tracker_announce(error_code const& ec) try
	{
		COMPLETE_ASYNC("tracker::on_tracker_announce");
		TORRENT_ASSERT(is_single_thread());
		TORRENT_ASSERT(m_waiting_tracker > 0);
		--m_waiting_tracker;
		if (ec) return;
		if (m_abort) return;
		announce_with_tracker();
	}
	catch (...) { handle_exception(); }

	void torrent::lsd_announce()
	{
	}

#ifndef TORRENT_DISABLE_DHT

	void torrent::dht_announce()
	{
	}

	void torrent::on_dht_announce_response_disp(std::weak_ptr<torrent> const t
		, protocol_version const v, std::vector<tcp::endpoint> const& peers)
	{
		std::shared_ptr<torrent> tor = t.lock();
		if (!tor) return;
		tor->on_dht_announce_response(v, peers);
	}

	void torrent::on_dht_announce_response(protocol_version const v
		, std::vector<tcp::endpoint> const& peers) try
	{
	}
	catch (...) { handle_exception(); }

#endif

	namespace
	{
		struct announce_protocol_state
		{
			// the tier is kept as INT_MAX until we find the first
			// tracker that works, then it's set to that tracker's
			// tier.
			int tier = INT_MAX;

			// have we sent an announce in this tier yet?
			bool sent_announce = false;

			// have we finished sending announces on this listen socket?
			bool done = false;
		};

		struct announce_state
		{
			explicit announce_state(aux::listen_socket_handle s)
				: socket(std::move(s)) {}

			aux::listen_socket_handle socket;

			aux::array<announce_protocol_state, num_protocols, protocol_version> state;
		};
	}

	void torrent::announce_with_tracker(event_t e)
	{
	}

	void torrent::scrape_tracker(int idx, bool const user_triggered)
	{
	}

	void torrent::tracker_warning(tracker_request const& req, std::string const& msg)
	{
		TORRENT_ASSERT(is_single_thread());

		INVARIANT_CHECK;

		protocol_version const hash_version = req.info_hash == m_info_hash.v1
			? protocol_version::V1 : protocol_version::V2;

		aux::announce_entry* ae = find_tracker(req.url);
		tcp::endpoint local_endpoint;
		if (ae)
		{
			for (auto& aep : ae->endpoints)
			{
				if (aep.socket != req.outgoing_socket) continue;
				local_endpoint = aep.local_endpoint;
				aep.info_hashes[hash_version].message = msg;
				break;
			}
		}
	}

	void torrent::tracker_scrape_response(tracker_request const& req
		, int const complete, int const incomplete, int const downloaded, int /* downloaders */)
	{
		TORRENT_ASSERT(is_single_thread());

		INVARIANT_CHECK;
		TORRENT_ASSERT(req.kind & tracker_request::scrape_request);

		protocol_version const hash_version = req.info_hash == m_info_hash.v1
			? protocol_version::V1 : protocol_version::V2;

		aux::announce_entry* ae = find_tracker(req.url);
		tcp::endpoint local_endpoint;
		if (ae)
		{
			auto* aep = ae->find_endpoint(req.outgoing_socket);
			if (aep)
			{
				local_endpoint = aep->local_endpoint;
				if (incomplete >= 0) aep->info_hashes[hash_version].scrape_incomplete = incomplete;
				if (complete >= 0) aep->info_hashes[hash_version].scrape_complete = complete;
				if (downloaded >= 0) aep->info_hashes[hash_version].scrape_downloaded = downloaded;

				update_scrape_state();
			}
		}

	}

	void torrent::update_scrape_state()
	{
		// loop over all trackers and find the largest numbers for each scrape field
		// then update the torrent-wide understanding of number of downloaders and seeds
		int complete = -1;
		int incomplete = -1;
		int downloaded = -1;
		for (auto const& t : m_trackers)
		{
			for (auto const& aep : t.endpoints)
			{
				for (protocol_version const ih : all_versions)
				{
					auto const& a = aep.info_hashes[ih];
					complete = std::max(a.scrape_complete, complete);
					incomplete = std::max(a.scrape_incomplete, incomplete);
					downloaded = std::max(a.scrape_downloaded, downloaded);
				}
			}
		}

		if ((complete >= 0 && int(m_complete) != complete)
			|| (incomplete >= 0 && int(m_incomplete) != incomplete)
			|| (downloaded >= 0 && int(m_downloaded) != downloaded))
			state_updated();

		if (int(m_complete) != complete
			|| int(m_incomplete) != incomplete
			|| int(m_downloaded) != downloaded)
		{
			m_complete = std::uint32_t(complete);
			m_incomplete = std::uint32_t(incomplete);
			m_downloaded = std::uint32_t(downloaded);

			update_auto_sequential();

			// these numbers are cached in the resume data
			set_need_save_resume();
		}
	}

	void torrent::tracker_response(
		tracker_request const& r
		, address const& tracker_ip // this is the IP we connected to
		, std::list<address> const& tracker_ips // these are all the IPs it resolved to
		, struct tracker_response const& resp)
	{
	}

	void torrent::update_auto_sequential()
	{
	}

	void torrent::do_connect_boost()
	{
		if (m_connect_boost_counter == 0) return;

	}


#if TORRENT_ABI_VERSION == 1
	void torrent::set_tracker_login(std::string const& name
		, std::string const& pw)
	{
		m_username = name;
		m_password = pw;
	}
#endif

	void torrent::on_peer_name_lookup(error_code const& e
		, std::vector<address> const& host_list, int const port
		, protocol_version const v) try
	{
	}
	catch (...) { handle_exception(); }

	std::optional<std::int64_t> torrent::bytes_left() const
	{

		std::int64_t left = 0;

		return left;
	}

	void torrent::on_piece_verified(aux::vector<sha256_hash> block_hashes
		, piece_index_t const piece
		, sha1_hash const& piece_hash, storage_error const& error) try
	{
	}
	catch (...) { handle_exception(); }

	void torrent::add_suggest_piece(piece_index_t const index)
	{
	}

	// this is called once we have completely downloaded piece
	// 'index', its hash has been verified. It's also called
	// during initial file check when we find a piece whose hash
	// is correct
	void torrent::we_have(piece_index_t const index)
	{
	}

	boost::tribool torrent::on_blocks_hashed(piece_index_t const piece
		, span<sha256_hash const> const block_hashes)
	{
		boost::tribool ret = boost::indeterminate;
		return ret;
	}

	// this is called when the piece hash is checked as correct. Note
	// that the piece picker and the torrent won't necessarily consider
	// us to have this piece yet, since it might not have been flushed
	// to disk yet. Only if we have predictive_piece_announce on will
	// we announce this piece to peers at this point.
	void torrent::piece_passed(piece_index_t const index)
	{
	}

#ifndef TORRENT_DISABLE_PREDICTIVE_PIECES
	// we believe we will complete this piece very soon
	// announce it to peers ahead of time to eliminate the
	// round-trip times involved in announcing it, requesting it
	// and sending it
	void torrent::predicted_have_piece(piece_index_t const index, time_duration const duration)
	{
	}
#endif

	// blocks may contain the block indices of the blocks that failed (if this is
	// a v2 torrent).
	void torrent::piece_failed(piece_index_t const index, std::vector<int> blocks)
	{
	}

	void torrent::on_piece_sync(piece_index_t const piece, std::vector<int> const& blocks) try
	{
	}
	catch (...) { handle_exception(); }

	void torrent::abort()
	{
		TORRENT_ASSERT(is_single_thread());

		if (m_abort) return;

		m_abort = true;
		update_want_peers();
		update_want_tick();
		update_want_scrape();
		update_gauge();
		stop_announcing();

	}

	// this is called when we're destructing non-gracefully. i.e. we're _just_
	// destructing everything.
	void torrent::panic()
	{
	}

#ifndef TORRENT_DISABLE_SUPERSEEDING
	void torrent::set_super_seeding(bool const on)
	{
	}

	// TODO: 3 this should return optional<>. piece index -1 should not be
	// allowed
	piece_index_t torrent::get_piece_to_super_seed(typed_bitfield<piece_index_t> const& bits)
	{
		// return a piece with low availability that is not in
		// the bitfield and that is not currently being super
		// seeded by any peer
		TORRENT_ASSERT(m_super_seeding);

		// do a linear search from the first piece
		int min_availability = 9999;
		std::vector<piece_index_t> avail_vec;
		if (avail_vec.empty()) return piece_index_t{-1};
		return avail_vec[random(std::uint32_t(avail_vec.size() - 1))];
	}
#endif

	void torrent::on_files_deleted(storage_error const& error) try
	{
		TORRENT_ASSERT(is_single_thread());
	}
	catch (...) { handle_exception(); }

	void torrent::on_file_renamed(std::string const& filename
		, file_index_t const file_idx
		, storage_error const& error) try
	{
	}
	catch (...) { handle_exception(); }

	void torrent::on_torrent_paused() try
	{
		TORRENT_ASSERT(is_single_thread());
	}
	catch (...) { handle_exception(); }

#if TORRENT_ABI_VERSION == 1
	std::string torrent::tracker_login() const
	{
		if (m_username.empty() && m_password.empty()) return "";
		return m_username + ":" + m_password;
	}
#endif

	std::uint32_t torrent::tracker_key() const
	{
		auto const self = reinterpret_cast<uintptr_t>(this);
		auto const ses = reinterpret_cast<uintptr_t>(&m_ses);
		std::uint32_t const storage = m_storage
			? static_cast<std::uint32_t>(static_cast<storage_index_t>(m_storage))
			: 0;
		sha1_hash const h = hasher(reinterpret_cast<char const*>(&self), sizeof(self))
			.update(reinterpret_cast<char const*>(&storage), sizeof(storage))
			.update(reinterpret_cast<char const*>(&ses), sizeof(ses))
			.final();
		unsigned char const* ptr = &h[0];
		return aux::read_uint32(ptr);
	}

#ifndef TORRENT_DISABLE_STREAMING
	void torrent::cancel_non_critical()
	{
	}

	void torrent::reset_piece_deadline(piece_index_t piece)
	{
		remove_time_critical_piece(piece);
	}

	void torrent::remove_time_critical_piece(piece_index_t const piece, bool const finished)
	{
	}

	void torrent::clear_time_critical()
	{
	}

	// remove time critical pieces where priority is 0
	void torrent::remove_time_critical_pieces(aux::vector<download_priority_t, piece_index_t> const& priority)
	{
		for (auto i = m_time_critical_pieces.begin(); i != m_time_critical_pieces.end();)
		{
			if (priority[i->piece] == dont_download)
			{
				i = m_time_critical_pieces.erase(i);
				continue;
			}
			++i;
		}
	}
#endif // TORRENT_DISABLE_STREAMING

	void torrent::piece_availability(aux::vector<int, piece_index_t>& avail) const
	{
	}

	void torrent::prioritize_piece_list(std::vector<std::pair<piece_index_t
		, download_priority_t>> const& pieces)
	{
	}

	void torrent::prioritize_pieces(aux::vector<download_priority_t, piece_index_t> const& pieces)
	{
	}

	void torrent::on_file_priority(storage_error const& err
		, aux::vector<download_priority_t, file_index_t> prios)
	{
	}

	void torrent::prioritize_files(aux::vector<download_priority_t, file_index_t> files)
	{
	}

	void torrent::set_file_priority(file_index_t const index
		, download_priority_t prio)
	{
	}

	download_priority_t torrent::file_priority(file_index_t const index) const
	{
		return m_file_priority[index];
	}

	void torrent::file_priorities(aux::vector<download_priority_t, file_index_t>* files) const
	{
	}

	// this is called when piece priorities have been updated
	// updates the interested flag in peers
	void torrent::update_peer_interest(bool const was_finished)
	{
	}

	std::vector<lt::announce_entry> torrent::trackers() const
	{
		std::vector<lt::announce_entry> ret;
		return ret;
	}

	void torrent::replace_trackers(std::vector<lt::announce_entry> const& urls)
	{
	}

	void torrent::prioritize_udp_trackers()
	{
	}

	bool torrent::add_tracker(lt::announce_entry const& url)
	{
		return true;
	}

	void torrent::trigger_unchoke() noexcept
	{
	}

	void torrent::trigger_optimistic_unchoke() noexcept
	{
	}

#ifdef TORRENT_SSL_PEERS
	// certificate is a filename to a .pem file which is our
	// certificate. The certificate must be signed by the root
	// cert of the torrent file. any peer we connect to or that
	// connect to use must present a valid certificate signed
	// by the torrent root cert as well
	void torrent::set_ssl_cert(std::string const& certificate
		, std::string const& private_key
		, std::string const& dh_params
		, std::string const& passphrase)
	{
		if (!m_ssl_ctx)
		{
			return;
		}

		error_code ec;
		m_ssl_ctx->set_password_callback(
				[passphrase](std::size_t, ssl::context::password_purpose purpose)
				{
					return purpose == ssl::context::for_reading ? passphrase : "";
				}
				, ec);
		m_ssl_ctx->use_certificate_file(certificate, ssl::context::pem, ec);
#ifndef TORRENT_DISABLE_LOGGING
		if (should_log())
			debug_log("*** use certificate file: %s", ec.message().c_str());
#endif
		m_ssl_ctx->use_private_key_file(private_key, ssl::context::pem, ec);
#ifndef TORRENT_DISABLE_LOGGING
		if (should_log())
			debug_log("*** use private key file: %s", ec.message().c_str());
#endif
		m_ssl_ctx->use_tmp_dh_file(dh_params, ec);
#ifndef TORRENT_DISABLE_LOGGING
		if (should_log())
			debug_log("*** use DH file: %s", ec.message().c_str());
#endif
	}

	void torrent::set_ssl_cert_buffer(std::string const& certificate
		, std::string const& private_key
		, std::string const& dh_params)
	{
		if (!m_ssl_ctx) return;

		boost::asio::const_buffer certificate_buf(certificate.c_str(), certificate.size());

		error_code ec;
		m_ssl_ctx->use_certificate(certificate_buf, ssl::context::pem, ec);

		boost::asio::const_buffer private_key_buf(private_key.c_str(), private_key.size());
		m_ssl_ctx->use_private_key(private_key_buf, ssl::context::pem, ec);

		boost::asio::const_buffer dh_params_buf(dh_params.c_str(), dh_params.size());
		m_ssl_ctx->use_tmp_dh(dh_params_buf, ec);
	}

#endif

	void torrent::on_exception(std::exception const&)
	{
	}

	void torrent::on_error(error_code const& ec)
	{
	}

	void torrent::on_remove_peers() noexcept
	{
	}

	void torrent::verify_block_hashes(piece_index_t index)
	{
	}

	std::vector<std::vector<sha256_hash>> torrent::get_piece_layers() const
	{
		std::vector<std::vector<sha256_hash>> ret;
		return ret;
	}

	void torrent::enable_all_trackers()
	{
		for (aux::announce_entry& ae : m_trackers)
			for (aux::announce_endpoint& aep : ae.endpoints)
				aep.enabled = true;
	}

#if defined TORRENT_SSL_PEERS
	struct hostname_visitor
	{
		explicit hostname_visitor(std::string const& h) : hostname_(h) {}
		template <typename T>
		void operator()(T&) {}
		template <typename T>
		void operator()(ssl_stream<T>& s) { s.set_host_name(hostname_); }
		std::string const& hostname_;
	};

	struct ssl_handle_visitor
	{
		template <typename T>
		ssl::stream_handle_type operator()(T&)
		{ return nullptr; }

		template <typename T>
		ssl::stream_handle_type operator()(ssl_stream<T>& s)
		{ return s.handle(); }
	};
#endif

	void torrent::initialize_merkle_trees()
	{
	}

	bool torrent::set_metadata(span<char const> metadata_buf)
	{
		return true;
	}

	bool torrent::want_tick() const
	{
		return false;
	}

	void torrent::update_want_tick()
	{
		update_list(aux::session_interface::torrent_want_tick, want_tick());
	}

	// this function adjusts which lists this torrent is part of (checking,
	// seeding or downloading)
	void torrent::update_state_list()
	{
	}

	// returns true if this torrent is interested in connecting to more peers
	bool torrent::want_peers() const
	{
		return true;
	}

	bool torrent::want_peers_download() const
	{
	}

	bool torrent::want_peers_finished() const
	{
	}

	void torrent::update_want_peers()
	{
		update_list(aux::session_interface::torrent_want_peers_download, want_peers_download());
		update_list(aux::session_interface::torrent_want_peers_finished, want_peers_finished());
	}

	void torrent::update_want_scrape()
	{
		update_list(aux::session_interface::torrent_want_scrape
			, m_paused && m_auto_managed && !m_abort);
	}

	void torrent::update_list(torrent_list_index_t const list, bool in)
	{

	}

	void torrent::disconnect_all(error_code const& ec, operation_t op)
	{
	}

	int torrent::disconnect_peers(int const num, error_code const& ec)
	{
		return 0;
	}

	// called when torrent is finished (all interesting
	// pieces have been downloaded)
	void torrent::finished()
	{
	}

	// this is called when we were finished, but some files were
	// marked for downloading, and we are no longer finished
	void torrent::resume_download()
	{
	}

	void torrent::maybe_done_flushing()
	{
	}

	// called when torrent is complete. i.e. all pieces downloaded
	// not necessarily flushed to disk
	void torrent::completed()
	{
	}

	int torrent::deprioritize_tracker(int index)
	{
		INVARIANT_CHECK;

		TORRENT_ASSERT(index >= 0);
		TORRENT_ASSERT(index < int(m_trackers.size()));
		if (index >= int(m_trackers.size())) return -1;

		while (index < int(m_trackers.size()) - 1 && m_trackers[index].tier == m_trackers[index + 1].tier)
		{
			using std::swap;
			swap(m_trackers[index], m_trackers[index + 1]);
			if (m_last_working_tracker == index) ++m_last_working_tracker;
			else if (m_last_working_tracker == index + 1) --m_last_working_tracker;
			++index;
		}
		return index;
	}

	void torrent::files_checked()
	{
	}

	aux::alert_manager& torrent::alerts() const
	{
		TORRENT_ASSERT(is_single_thread());
		return m_ses.alerts();
	}

	bool torrent::is_seed() const
	{
		return true;
	}

	bool torrent::is_finished() const
	{
		return true;
	}

	bool torrent::is_inactive() const
	{
		return false;
	}

	std::string torrent::save_path() const
	{
		return m_save_path;
	}

	void torrent::rename_file(file_index_t const index, std::string name)
	{
	}

	void torrent::on_storage_moved(status_t const status, std::string const& path
		, storage_error const& error) try
	{
		TORRENT_ASSERT(is_single_thread());

		m_moving_storage = false;
		if (status == status_t::no_error
			|| status == status_t::need_full_check)
		{
			m_save_path = path;
			set_need_save_resume();
			if (status == status_t::need_full_check)
				force_recheck();
		}
	}
	catch (...) { handle_exception(); }

	aux::session_settings const& torrent::settings() const
	{
		TORRENT_ASSERT(is_single_thread());
		return m_ses.settings();
	}

#if TORRENT_USE_INVARIANT_CHECKS
	void torrent::check_invariant() const
	{
	}
#endif

	void torrent::set_sequential_download(bool const sd)
	{
	}

	void torrent::queue_up()
	{
	}

	void torrent::queue_down()
	{
		set_queue_position(next(queue_position()));
	}

	void torrent::set_queue_position(queue_position_t const p)
	{
		TORRENT_ASSERT(is_single_thread());

		// finished torrents may not change their queue positions, as it's set to
		// -1
		if ((m_abort || is_finished()) && p != no_pos) return;

		TORRENT_ASSERT((p == no_pos) == is_finished()
			|| (!m_auto_managed && p == no_pos)
			|| (m_abort && p == no_pos)
			|| (!m_added && p == no_pos));
		if (p == m_sequence_number) return;

		TORRENT_ASSERT(p >= no_pos);

		state_updated();
	}

	void torrent::set_max_uploads(int limit, bool const state_update)
	{
	}

	void torrent::set_max_connections(int limit, bool const state_update)
	{
	}

	void torrent::set_upload_limit(int const limit)
	{
	}

	void torrent::set_download_limit(int const limit)
	{
	}

	void torrent::set_limit_impl(int limit, int const channel, bool const state_update)
	{
	}

	void torrent::setup_peer_class()
	{
	}

	int torrent::limit_impl(int const channel) const
	{
        return 1;
	}

	int torrent::upload_limit() const
	{
		return 1;
	}

	int torrent::download_limit() const
	{
		return 1;
	}

	void torrent::clear_error()
	{
	}

	std::string torrent::resolve_filename(file_index_t const file) const
	{
		return "TAU";
	}

	void torrent::set_error(error_code const& ec, file_index_t const error_file)
	{
		TORRENT_ASSERT(is_single_thread());
		m_error = ec;
		m_error_file = error_file;

		update_gauge();

#ifndef TORRENT_DISABLE_LOGGING
		if (ec)
		{
			char buf[1024];
			std::snprintf(buf, sizeof(buf), "error %s: %s", ec.message().c_str()
				, resolve_filename(error_file).c_str());
			log_to_all_peers(buf);
		}
#endif

		state_updated();
		update_state_list();
	}

	void torrent::auto_managed(bool a)
	{
		TORRENT_ASSERT(is_single_thread());
		INVARIANT_CHECK;

		if (m_auto_managed == a) return;
		bool const checking_files = should_check_files();
		m_auto_managed = a;
		update_gauge();
		update_want_scrape();
		update_state_list();

		state_updated();

		// we need to save this new state as well
		set_need_save_resume();

		if (!checking_files && should_check_files())
		{
			start_checking();
		}
	}

	namespace {

	std::uint16_t clamped_subtract_u16(int const a, int const b)
	{
		if (a < b) return 0;
		return std::uint16_t(a - b);
	}

	} // anonymous namespace

	// this is called every time the session timer takes a step back. Since the
	// session time is meant to fit in 16 bits, it only covers a range of
	// about 18 hours. This means every few hours the whole epoch of this
	// clock is shifted forward. All timestamp in this clock must then be
	// shifted backwards to remain the same. Anything that's shifted back
	// beyond the new epoch is clamped to 0 (to represent the oldest timestamp
	// currently representable by the session_time)
	void torrent::step_session_time(int const seconds)
	{
	}

	// the higher seed rank, the more important to seed
	int torrent::seed_rank(aux::session_settings const& s) const
	{
		return 0;
	}

	bool torrent::should_check_files() const
	{
		return false;
	}

	void torrent::flush_cache()
	{
	}

	void torrent::on_cache_flushed(bool const manually_triggered) try
	{
		TORRENT_ASSERT(is_single_thread());

		if (m_ses.is_aborted()) return;

	}
	catch (...) { handle_exception(); }

	void torrent::on_torrent_aborted()
	{
		TORRENT_ASSERT(is_single_thread());

		// there should be no more disk activity for this torrent now, we can
		// release the disk io handle
		m_storage.reset();
	}

	bool torrent::is_paused() const
	{
		return m_paused || m_session_paused;
	}

#ifndef TORRENT_DISABLE_LOGGING
	void torrent::log_to_all_peers(char const* message)
	{
	}
#endif

	void torrent::set_session_paused(bool const b)
	{
	}

	void torrent::resume()
	{
		TORRENT_ASSERT(is_single_thread());
		INVARIANT_CHECK;

		if (!m_paused
			&& m_announce_to_dht
			&& m_announce_to_trackers
			&& m_announce_to_lsd) return;

		m_announce_to_dht = true;
		m_announce_to_trackers = true;
		m_announce_to_lsd = true;
		m_paused = false;
		if (!m_session_paused) m_graceful_pause_mode = false;

		update_gauge();

		// we need to save this new state
		set_need_save_resume();

		do_resume();
	}

	void torrent::do_resume()
	{
	}

	namespace
	{
		struct timer_state
		{
			explicit timer_state(aux::listen_socket_handle s)
				: socket(std::move(s)) {}

			aux::listen_socket_handle socket;

			struct state_t
			{
				int tier = INT_MAX;
				bool found_working = false;
				bool done = false;
			};
			aux::array<state_t, num_protocols, protocol_version> state;
		};
	}

	void torrent::update_tracker_timer(time_point32 const now)
	{
	}

	void torrent::start_announcing()
	{
	}

	void torrent::stop_announcing()
	{
		TORRENT_ASSERT(is_single_thread());
		if (!m_announcing) return;

		m_tracker_timer.cancel();

		m_announcing = false;

		time_point32 const now = aux::time_now32();
		for (auto& t : m_trackers)
		{
			for (auto& aep : t.endpoints)
			{
				for (auto& a : aep.info_hashes)
				{
					a.next_announce = now;
					a.min_announce = now;
				}
			}
		}
		announce_with_tracker(event_t::stopped);
	}

	seconds32 torrent::finished_time() const
	{
		if(!is_finished() || is_paused())
			return m_finished_time;

		return m_finished_time + duration_cast<seconds32>(
			aux::time_now() - m_became_finished);
	}

	seconds32 torrent::active_time() const
	{
		if (is_paused())
			return m_active_time;

		// m_active_time does not account for the current "session", just the
		// time before we last started this torrent. To get the current time, we
		// need to add the time since we started it
		return m_active_time + duration_cast<seconds32>(
			aux::time_now() - m_started);
	}

	seconds32 torrent::seeding_time() const
	{
		if(!is_seed() || is_paused())
			return m_seeding_time;
		// m_seeding_time does not account for the current "session", just the
		// time before we last started this torrent. To get the current time, we
		// need to add the time since we started it
		return m_seeding_time + duration_cast<seconds32>(
			aux::time_now() - m_became_seed);
	}

	seconds32 torrent::upload_mode_time() const
	{
		if(!m_upload_mode)
			return seconds32(0);

		return aux::time_now32() - m_upload_mode_time;
	}

	void torrent::second_tick(int const tick_interval_ms)
	{
	}

	bool torrent::is_inactive_internal() const
	{
		if (is_finished())
			return m_stat.upload_payload_rate()
				< settings().get_int(settings_pack::inactive_up_rate);
		else
			return m_stat.download_payload_rate()
				< settings().get_int(settings_pack::inactive_down_rate);
	}

	void torrent::on_inactivity_tick(error_code const& ec) try
	{
		m_pending_active_change = false;

		if (ec) return;

		bool const is_inactive = is_inactive_internal();
		if (is_inactive == m_inactive) return;

		m_inactive = is_inactive;

		update_state_list();
		update_want_tick();

	}
	catch (...) { handle_exception(); }

	namespace {
		int zero_or(int const val, int const def_val)
		{ return (val <= 0) ? def_val : val; }
	}

	void torrent::maybe_connect_web_seeds()
	{
	}

#ifndef TORRENT_DISABLE_SHARE_MODE
	void torrent::recalc_share_mode()
	{
	}
#endif // TORRENT_DISABLE_SHARE_MODE

	void torrent::sent_bytes(int const bytes_payload, int const bytes_protocol)
	{
		m_stat.sent_bytes(bytes_payload, bytes_protocol);
		m_ses.sent_bytes(bytes_payload, bytes_protocol);
	}

	void torrent::received_bytes(int const bytes_payload, int const bytes_protocol)
	{
		m_stat.received_bytes(bytes_payload, bytes_protocol);
		m_ses.received_bytes(bytes_payload, bytes_protocol);
	}

	void torrent::trancieve_ip_packet(int const bytes, bool const ipv6)
	{
		m_stat.trancieve_ip_packet(bytes, ipv6);
		m_ses.trancieve_ip_packet(bytes, ipv6);
	}

	void torrent::sent_syn(bool const ipv6)
	{
		m_stat.sent_syn(ipv6);
		m_ses.sent_syn(ipv6);
	}

	void torrent::received_synack(bool const ipv6)
	{
		m_stat.received_synack(ipv6);
		m_ses.received_synack(ipv6);
	}

	std::set<std::string> torrent::web_seeds() const
	{
		TORRENT_ASSERT(is_single_thread());
		std::set<std::string> ret;
		return ret;
	}

	bool torrent::try_connect_peer()
	{
		return true;
	}

	// verify piece is used when checking resume data or when the user
	// adds a piece
	void torrent::verify_piece(piece_index_t const piece)
	{
	}

	aux::announce_entry* torrent::find_tracker(std::string const& url)
	{
		auto i = std::find_if(m_trackers.begin(), m_trackers.end()
			, [&url](aux::announce_entry const& ae) { return ae.url == url; });
		if (i == m_trackers.end()) return nullptr;
		return &*i;
	}

	void torrent::ip_filter_updated()
	{
	}

	void torrent::port_filter_updated()
	{
	}

	void torrent::new_external_ip()
	{
	}

	void torrent::stop_when_ready(bool const b)
	{
		m_stop_when_ready = b;

		// to avoid race condition, if we're already in a downloading state,
		// trigger the stop-when-ready logic immediately.
		if (m_stop_when_ready && is_downloading_state(m_state))
		{
#ifndef TORRENT_DISABLE_LOGGING
			debug_log("stop_when_ready triggered");
#endif
			auto_managed(false);
			pause();
			m_stop_when_ready = false;
		}
	}

	void torrent::state_updated()
	{
	}

	int torrent::priority() const
	{
		return 1;
	}

#if TORRENT_ABI_VERSION == 1
	void torrent::set_priority(int const prio)
	{
		// priority 1 is default
		if (prio == 1 && m_peer_class == peer_class_t{}) return;

		if (m_peer_class == peer_class_t{})
			setup_peer_class();

		state_updated();
	}
#endif

	void torrent::add_redundant_bytes(int const b, waste_reason const reason)
	{
		TORRENT_ASSERT(is_single_thread());
		TORRENT_ASSERT(b > 0);
		TORRENT_ASSERT(static_cast<int>(reason) >= 0);
		TORRENT_ASSERT(static_cast<int>(reason) < static_cast<int>(waste_reason::max));

		if (m_total_redundant_bytes <= std::numeric_limits<std::int64_t>::max() - b)
			m_total_redundant_bytes += b;
		else
			m_total_redundant_bytes = std::numeric_limits<std::int64_t>::max();

		// the stats counters are 64 bits, so we don't check for overflow there
		m_stats_counters.inc_stats_counter(counters::recv_redundant_bytes, b);
		m_stats_counters.inc_stats_counter(counters::waste_piece_timed_out + static_cast<int>(reason), b);
	}

	void torrent::add_failed_bytes(int const b)
	{
		TORRENT_ASSERT(is_single_thread());
		TORRENT_ASSERT(b > 0);
		if (m_total_failed_bytes <= std::numeric_limits<std::int64_t>::max() - b)
			m_total_failed_bytes += b;
		else
			m_total_failed_bytes = std::numeric_limits<std::int64_t>::max();

		// the stats counters are 64 bits, so we don't check for overflow there
		m_stats_counters.inc_stats_counter(counters::recv_failed_bytes, b);
	}

	// the number of connected peers that are seeds
	int torrent::num_seeds() const
	{
		TORRENT_ASSERT(is_single_thread());
		INVARIANT_CHECK;

		return int(m_num_seeds) - int(m_num_connecting_seeds);
	}

	// the number of connected peers that are not seeds
	int torrent::num_downloaders() const
	{
		return 0;
	}

	void torrent::tracker_request_error(tracker_request const& r
		, error_code const& ec, operation_t const op, std::string const& msg
		, seconds32 const retry_interval)
	{
	}

#ifndef TORRENT_DISABLE_LOGGING
	bool torrent::should_log() const
	{
		return true;
	}

	TORRENT_FORMAT(2,3)
	void torrent::debug_log(char const* fmt, ...) const noexcept try
	{
		va_list v;
		va_start(v, fmt);
		va_end(v);
	}
	catch (std::exception const&) {}
#endif

}
