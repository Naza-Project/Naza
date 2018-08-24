// Copyright (c) 2012-2018, The CryptoNote developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

// Copyright (c) 2018-2019, The Naza developers.
// Licensed under the GNU Lesser General Public License. See LICENSE for details.

#include "Config.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include "CryptoNoteConfig.hpp"
#include "common/Base64.hpp"
#include "platform/PathTools.hpp"
#include "platform/Time.hpp"

static void parse_peer_and_add_to_container(const std::string &str, std::vector<nazacoin::NetworkAddress> &container) {
	nazacoin::NetworkAddress na{};
	if (!common::parse_ip_address_and_port(str, &na.ip, &na.port))
		throw std::runtime_error("Wrong address format " + str + ", should be ip:port");
	container.push_back(na);
}

using namespace common;
using namespace nazacoin;

const static UUID NAZACOIN_NETWORK = {{0xAE, 0x33, 0x2B, 0x0C, 0x3D, 0x45, 0x61, 0x52, 0x1A, 0x26, 0xEE, 0xDA, 0x1C, 0x43, 0xE4, 0xB3}};  // Bender's nightmare
Config::Config(common::CommandLine &cmd)
    : is_testnet(cmd.get_bool("--testnet"))
    , blocks_file_name(parameters::CRYPTONOTE_BLOCKS_FILENAME)
    , block_indexes_file_name(parameters::CRYPTONOTE_BLOCKINDEXES_FILENAME)
    , crypto_note_name(CRYPTONOTE_NAME)
    , network_id(NAZACOIN_NETWORK)
    , p2p_bind_port(P2P_DEFAULT_PORT)
    , p2p_external_port(P2P_DEFAULT_PORT)
    , p2p_bind_ip("0.0.0.0")
    , nazad_bind_port(RPC_DEFAULT_PORT)
    , nazad_bind_ip("127.0.0.1") 
    , nazad_remote_port(0)
    , nazad_remote_ip("127.0.0.1")
    , walletd_bind_port(WALLET_RPC_DEFAULT_PORT)
    , walletd_bind_ip("127.0.0.1")  
    , p2p_local_white_list_limit(P2P_LOCAL_WHITE_PEERLIST_LIMIT)
    , p2p_local_gray_list_limit(P2P_LOCAL_GRAY_PEERLIST_LIMIT)
    , p2p_default_peers_in_handshake(P2P_DEFAULT_PEERS_IN_HANDSHAKE)
    , p2p_default_connections_count(P2P_DEFAULT_CONNECTIONS_COUNT)
    , p2p_allow_local_ip(is_testnet)
    , p2p_whitelist_connections_percent(P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT)
    , p2p_block_ids_sync_default_count(BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT)
    , p2p_blocks_sync_default_count(BLOCKS_SYNCHRONIZING_DEFAULT_COUNT)
    , rpc_get_blocks_fast_max_count(COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT) {
	common::pod_from_hex(P2P_STAT_TRUSTED_PUBLIC_KEY, trusted_public_key);

	if (is_testnet) {
		network_id.data[0] += 1;
		p2p_bind_port += 1000;
		p2p_external_port += 1000;
		nazad_bind_port += 1000;
		p2p_allow_local_ip = true;
		if (const char *pa = cmd.get("--time-multiplier"))
			platform::set_time_multiplier_for_tests(boost::lexical_cast<int>(pa));
	}
	if (const char *pa = cmd.get("--p2p-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, &p2p_bind_ip, &p2p_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--p2p-external-port"))
		p2p_external_port = boost::lexical_cast<uint16_t>(pa);
	if (const char *pa = cmd.get("--wallet-rpc-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, &walletd_bind_ip, &walletd_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--ssl-certificate-pem-file")) {
		ssl_certificate_pem_file = pa;
#if !platform_USE_SSL
		throw std::runtime_error(
		    "Setting --ssl-certificate-pem-file impossible - this binary is built without OpenSSL");
#endif
	}
	if (const char *pa = cmd.get("--ssl-certificate-password")) {
		ssl_certificate_password = pa;
#if !platform_USE_SSL
		throw std::runtime_error(
		    "Setting --ssl_certificate_password impossible - this binary is built without OpenSSL");
#endif
	}
	if (const char *pa = cmd.get("--rpc-authorization")) {
		nazad_authorization = common::base64::encode(BinaryArray(pa, pa + strlen(pa)));
	}
	if (const char *pa = cmd.get("--daemon-rpc-bind-address")) {
		if (!common::parse_ip_address_and_port(pa, &nazad_bind_ip, &nazad_bind_port))
			throw std::runtime_error("Wrong address format " + std::string(pa) + ", should be ip:port");
	}
	if (const char *pa = cmd.get("--daemon-remote-address")) {
		std::string addr         = pa;
		const std::string prefix = "https://";
		if (addr.find(prefix) == 0) {
#if !platform_USE_SSL
			throw std::runtime_error(
			    "Using https in --daemon-remote-address impossible - this binary is built without OpenSSL");
#endif
			std::string sip;
			std::string sport;
			if (!split_string(addr.substr(prefix.size()), ":", sip, sport))
				throw std::runtime_error(
				    "Wrong address format " + addr + ", should be <ip>:<port> or https://<host>:<port>");
			nazad_remote_port = boost::lexical_cast<uint16_t>(sport);
			nazad_remote_ip   = prefix + sip;
		} else {
			const std::string prefix2 = "http://";
			if (addr.find(prefix2) == 0)
				addr = addr.substr(prefix2.size());
			if (!common::parse_ip_address_and_port(addr, &nazad_remote_ip, &nazad_remote_port))
				throw std::runtime_error("Wrong address format " + addr + ", should be ip:port");
		}
	}
	if (cmd.get_bool("--allow-local-ip", "Local IPs are automatically allowed for peers from the same private network"))
		p2p_allow_local_ip = true;
	for (auto &&pa : cmd.get_array("--seed-node-address"))
		parse_peer_and_add_to_container(pa, seed_nodes);
	for (auto &&pa : cmd.get_array("--seed-node", "Use --seed-node-address instead"))
		parse_peer_and_add_to_container(pa, seed_nodes);
	for (auto &&pa : cmd.get_array("--priority-node-address"))
		parse_peer_and_add_to_container(pa, priority_nodes);
	for (auto &&pa : cmd.get_array("--add-priority-node", "Use --priority-node-address instead"))
		parse_peer_and_add_to_container(pa, priority_nodes);
	for (auto &&pa : cmd.get_array("--exclusive-node-address"))
		parse_peer_and_add_to_container(pa, exclusive_nodes);
	for (auto &&pa : cmd.get_array("--add-exclusive-node", "Use --exclusive-node-address instead"))
		parse_peer_and_add_to_container(pa, exclusive_nodes);

	if (seed_nodes.empty() && !is_testnet)
		for (auto &&sn : nazacoin::SEED_NODES) {
			NetworkAddress addr;
			if (!common::parse_ip_address_and_port(sn, &addr.ip, &addr.port))
				continue;
			seed_nodes.push_back(addr);
		}

	std::sort(seed_nodes.begin(), seed_nodes.end());
	std::sort(exclusive_nodes.begin(), exclusive_nodes.end());
	std::sort(priority_nodes.begin(), priority_nodes.end());

	data_folder = platform::get_app_data_folder(crypto_note_name);
	if (is_testnet)
		data_folder += "_testnet";
	if (const char *pa = cmd.get("--data-folder")) {
		data_folder = pa;
		if (!platform::folder_exists(data_folder))
			throw std::runtime_error("Data folder must exist " + data_folder);
	} else {
		if (!platform::create_folders_if_necessary(data_folder)) 
			throw std::runtime_error("Failed to create data folder " + data_folder);
	}
}

std::string Config::get_data_folder(const std::string &subdir) const {
	std::string folder = data_folder;
	folder += "/" + subdir;
	if (!platform::create_folder_if_necessary(folder))
		throw std::runtime_error("Failed to create coin folder " + folder);
	return folder;
}