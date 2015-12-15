/*

Copyright (c) 2015, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

namespace libtorrent
{
	namespace
	{
		void apply_flag(boost:uint32_t& current_flags
			, bdecode_node const& n
			, char const* name
			, const boost::uint32_t flag)
		{
			if (n.dict_find_int_value(name, -1) == -1)
			{
				current_flags &= ~flag;
			}
			else
			{
				current_flags |= flag;
			}
		}
	}

	add_torrent_params read_resume_data(bdecode_node const& rd, error_code& ec)
	{
		add_torrent_params ret;

		ret.total_uploaded = rd.dict_find_int_value("total_uploaded");
		ret.total_downloaded = rd.dict_find_int_value("total_downloaded");
		ret.active_time = rd.dict_find_int_value("active_time");
		ret.finished_time = rd.dict_find_int_value("finished_time");
		ret.seeding_time = rd.dict_find_int_value("seeding_time");

#error add fields for this
		last_seen_complete = rd.dict_find_int_value("last_seen_complete");
		complete = rd.dict_find_int_value("num_complete", 0xffffff);
		incomplete = rd.dict_find_int_value("num_incomplete", 0xffffff);
		downloaded = rd.dict_find_int_value("num_downloaded", 0xffffff);

		ret.upload_limit = rd.dict_find_int_value("upload_rate_limit", -1);
		ret.download_limit = rd.dict_find_int_value("download_rate_limit", -1);
		ret.max_connections = rd.dict_find_int_value("max_connections", -1);
		ret.max_uploads = rd.dict_find_int_value("max_uploads", -1);

		apply_flag(ret.flags, rd, "seed_mode", add_torrent_params::flag_seed_mode);
		apply_flag(ret.flags, rd, "super_seeding", add_torrent_params::flag_super_Seeding);
		apply_flag(ret.flags, rd, "auto_managed", add_torrent_params::flag_auto_managed);
		apply_flag(ret.flags, rd, "sequential_download", add_torrent_params::flag_sequential_download);
		apply_flag(ret.flags, rd, "paused", add_torrent_params::flag_paused);

#error add these to add_torrent_params
		int dht_ = rd.dict_find_int_value("announce_to_dht", -1);
		if (dht_ != -1) m_announce_to_dht = (dht_ != 0);
		int lsd_ = rd.dict_find_int_value("announce_to_lsd", -1);
		if (lsd_ != -1) m_announce_to_lsd = (lsd_ != 0);
		int track_ = rd.dict_find_int_value("announce_to_trackers", -1);
		if (track_ != -1) m_announce_to_trackers = (track_ != 0);

#error add fields for these
		int now = m_ses.session_time();
		int tmp = rd.dict_find_int_value("last_scrape", -1);
		m_last_scrape = tmp == -1 ? (std::numeric_limits<boost::int16_t>::min)() : now - tmp;
		tmp = rd.dict_find_int_value("last_download", -1);
		m_last_download = tmp == -1 ? (std::numeric_limits<boost::int16_t>::min)() : now - tmp;
		tmp = rd.dict_find_int_value("last_upload", -1);
		m_last_upload = tmp == -1 ? (std::numeric_limits<boost::int16_t>::min)() : now - tmp;

		ret.save_path = rd.dict_find_string_value("save_path");

		ret.url = rd.dict_find_string_value("url");
		ret.uuid = rd.dict_find_string_value("uuid");
		ret.source_feed_url = rd.dict_find_string_value("feed");

#error add a field for this
		bdecode_node mapped_files = rd.dict_find_list("mapped_files");
		if (mapped_files && mapped_files.list_size() == m_torrent_file->num_files())
		{
			for (int i = 0; i < m_torrent_file->num_files(); ++i)
			{
				std::string new_filename = mapped_files.list_string_value_at(i);
				if (new_filename.empty()) continue;
				m_torrent_file->rename_file(i, new_filename);
			}
		}

#error add fields for these
		m_added_time = rd.dict_find_int_value("added_time", m_added_time);
		m_completed_time = rd.dict_find_int_value("completed_time", m_completed_time);
		if (m_completed_time != 0 && m_completed_time < m_added_time)
			m_completed_time = m_added_time;

		// load file priorities except if the add_torrent_param file was set to
		// override resume data
		bdecode_node file_priority = rd.dict_find_list("file_priority");
		if (file_priority)
		{
			const int num_files = file_priority.list_size();
			ret.file_priorities.resize(num_files, 4);
			for (int i = 0; i < num_files; ++i)
			{
				ret.file_priorities[i] = file_priority.list_int_value_at(i, 1);
				// this is suspicious, leave seed mode
				if (ret.file_priorities[i] == 0)
				{
					ret.flags &= ~add_torrent_params::flags_seed_mode;
				}
			}
		}

		bdecode_node trackers = rd.dict_find_list("trackers");
		if (trackers)
		{
			// it's possible to delete the trackers from a torrent and then save
			// resume data with an empty trackers list. Since we found a trackers
			// list here, these should replace whatever we find in the .torrent
			// file.
			ret.flags &= ~add_torrent_params::flag_merge_resume_trackers;

			int tier = 0;
			for (int i = 0; i < trackers.list_size(); ++i)
			{
				bdecode_node tier_list = trackers.list_at(i);
				if (!tier_list || tier_list.type() != bdecode_node::list_t)
					continue;

				for (int j = 0; j < tier_list.list_size(); ++j)
				{
					ret.trackers.push_back(tier_list.list_string_value_at(j));
					ret.tracker_tiers.push_back(tier);
				}
				++tier;
			}
			std::sort(m_trackers.begin(), m_trackers.end(), boost::bind(&announce_entry::tier, _1)
				< boost::bind(&announce_entry::tier, _2));

			if (settings().get_bool(settings_pack::prefer_udp_trackers))
				prioritize_udp_trackers();
		}

		// if merge resume http seeds is not set, we need to clear whatever web
		// seeds we loaded from the .torrent file, because we want whatever's in
		// the resume file to take precedence. If there aren't even any fields in
		// the resume data though, keep the ones from the torrent
		bdecode_node url_list = rd.dict_find_list("url-list");
		bdecode_node httpseeds = rd.dict_find_list("httpseeds");
		if ((url_list || httpseeds) && !m_merge_resume_http_seeds)
		{
			// since we found http seeds in the resume data, they should replace
			// whatever web seeds are specified in the .torrent
			ret.flags &= ~add_torrent_params::flag_merge_resume_http_seeds;
		}

		if (url_list)
		{
			for (int i = 0; i < url_list.list_size(); ++i)
			{
				std::string url = url_list.list_string_value_at(i);
				if (url.empty()) continue;
				ret.url_seeds.push_back(url);

#error this correction logic has to be moved to the torrent constructor now
				if (m_torrent_file->num_files() > 1 && url[url.size()-1] != '/') url += '/';
			}
		}

		if (httpseeds)
		{
			for (int i = 0; i < httpseeds.list_size(); ++i)
			{
				std::string url = httpseeds.list_string_value_at(i);
				if (url.empty()) continue;
#error add this field (merge with url_seeds?)
				ret.http_seeds.push_back(url);
			}
		}

		bdecode_node mt = rd.dict_find_string("merkle tree");
		if (mt)
		{
#error add field for this
			std::vector<sha1_hash> tree;
			tree.resize(m_torrent_file->merkle_tree().size());
			std::memcpy(&tree[0], mt.string_ptr()
				, (std::min)(mt.string_length(), int(tree.size()) * 20));
			if (mt.string_length() < int(tree.size()) * 20)
				std::memset(&tree[0] + mt.string_length() / 20, 0
					, tree.size() - mt.string_length() / 20);
			m_torrent_file->set_merkle_tree(tree);
		}


#error this is the case where the torrent is a merkle torrent but the resume \
		data does not contain the merkle tree, we need some kind of check in the \
		torrent constructor and error reporting
		{
			// TODO: 0 if this is a merkle torrent and we can't
			// restore the tree, we need to wipe all the
			// bits in the have array, but not necessarily
			// we might want to do a full check to see if we have
			// all the pieces. This is low priority since almost
			// no one uses merkle torrents
			TORRENT_ASSERT(false);
		}

#error add fields for these
		// some sanity checking. Maybe we shouldn't be in seed mode anymore
		bdecode_node pieces = rd.dict_find("pieces");
		if (pieces && pieces.type() == bdecode_node::string_t
			&& int(pieces.string_length()) == m_torrent_file->num_pieces())
		{
			char const* pieces_str = pieces.string_ptr();
			for (int i = 0, end(pieces.string_length()); i < end; ++i)
			{
				// being in seed mode and missing a piece is not compatible.
				// Leave seed mode if that happens
				if ((pieces_str[i] & 1)) continue;
				m_seed_mode = false;
				break;
			}
		}

		bdecode_node piece_priority = rd.dict_find_string("piece_priority");
		if (piece_priority && piece_priority.string_length()
			== m_torrent_file->num_pieces())
		{
			char const* p = piece_priority.string_ptr();
			for (int i = 0; i < piece_priority.string_length(); ++i)
			{
				if (p[i] > 0) continue;
				m_seed_mode = false;
				break;
			}
		}

		m_verified.resize(m_torrent_file->num_pieces(), false);

		return ret;
	}

	add_torrent_params read_resume_data(char const* buffer, int size, error_code& ec)
	{
		bdecode_node rd;
		bdecode(buffer, buffer + size, rd, ec);
		if (ec) return add_torrent_params();

		return read_resume_data(rd, ec);
	}
}

