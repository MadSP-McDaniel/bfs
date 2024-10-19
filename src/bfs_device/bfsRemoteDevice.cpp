/**
*
* @file   bfsRemoteDevice.cpp
* @brief  This is the class implementation for the storage device interface for
		  the bfs file system (non-device side).  This is the server side, where
		  eachobject represents a device on the network/in a process.
*
*/

/* Include files  */
#include <string.h>

/* Project include files */
#include <bfsDeviceLayer.h>
#include <bfsRemoteDevice.h>
#include <bfs_log.h>
#include <bfs_server.h>

#ifdef __BFS_ENCLAVE_MODE
#include "bfs_enclave_t.h" /* For ocalls */
#else
#include <bfs_util_ocalls.h>
#endif
// just include directly for nonenclave/denbug code
// #include <bfs_util_ocalls.h>

/* For performance testing (in enclave only) */
#ifdef __BFS_ENCLAVE_MODE
int num_reads = 0;	// number of device block reads per fs-level operation
int num_writes = 0; // number of device block writes per fs-level operation
bool collect_core_lats = true;
std::vector<long> s_read__net_d_send_lats, s_write__net_d_send_lats,
	s_other__net_d_send_lats;
int op = -1;
double f_start_time = 0.0, f_end_time = 0.0;
#endif

/* Macros */

/* Globals  */

//
// Class Data

//
// Class Functions

/**
 * @brief The attribute constructor for the class
 *
 * @param address - the address to connect to
 * @param port - the port to bind to for incoming connections
 * @return int : 0 is success, -1 is failure
 */

bfsRemoteDevice::bfsRemoteDevice(string address, unsigned short port)
	: devState(BFSDEV_UKNOWN), deviceID(0), numBlocks(0), commAddress(address),
	  commPort(port), remoteConn(NULL), remoteMux(NULL), secContext(NULL),
	  rd_send_seq(0), rd_recv_seq(0) {

	// Return, no return code
	return;
}

/**
 * @brief The destructor function for the class
 *
 * @param none
 */

bfsRemoteDevice::~bfsRemoteDevice(void) {

	// Clean up the device objects
	delete secContext;

	// Return, no return code
	return;
}

/**
 * @brief Initialize the device
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::bfsDeviceInitialize(void) {

	// Create the request for the remote device information
	bfs_uid_t usr = 1;
	bfs_device_id_t did = 0;
	bfs_device_msg_t cmd;
	bfsConnectionList ready;
	bfs_device_topo_t *topo;
	bfsFlexibleBuffer buf;
	bool ack;

	// Now setup the server connection
	logMessage(DEVICE_LOG_LEVEL,
			   "Attempting connection to remote device [%s/%d]",
			   commAddress.c_str(), commPort);
	remoteConn = bfsNetworkConnection::bfsChannelFactory(commAddress, commPort);
	if (remoteConn == NULL) {
		logMessage(LOG_ERROR_LEVEL,
				   "Remote device connection create failed, aborting.");
		changeDeviceState(BFSDEV_ERRORED);
		return (-1);
	}

	// Connect the server for incoming connections
	if (remoteConn->connect()) {
		logMessage(LOG_ERROR_LEVEL, "Remote device connect failed, aborting.");
		changeDeviceState(BFSDEV_ERRORED);
		return (-1);
	}

	// Create the mux for communication
	remoteMux = new bfsConnectionMux();
	remoteMux->addConnection(remoteConn);

	// Marshal the packet
	if (bfsDeviceLayer::marshalBfsDevicePacket(usr, did, BFS_DEVICE_GET_TOPO, 0,
											   secContext, rd_send_seq,
											   buf) == -1) {
		return (-1);
	}
	rd_send_seq++;

	// Send the packet
	if (remoteConn->sendPacketizedBuffer(buf) != (int)buf.getLength()) {
		logMessage(LOG_ERROR_LEVEL,
				   "Remote device topo request send failed, abort.");
		return (-1);
	}

	// Receive the response packet
	if ((remoteMux->waitConnections(ready, 0)) ||
		(ready.find(remoteConn->getSocket()) == ready.end()) ||
		(remoteConn->recvPacketizedBuffer(buf) <= 0)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Remote device topo request recv failed, abort.");
		return (-1);
	}

	// Unmarshal the data, sanity check it
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, buf) == -1) ||
		(usr != 1) || (cmd != BFS_DEVICE_GET_TOPO) || (ack != 1) ||
		(buf.getLength() != sizeof(bfs_device_topo_t))) {
		logMessage(LOG_ERROR_LEVEL,
				   "Remote device topo request bad data response, abort.");
		return (-1);
	}
	rd_recv_seq++;

	// Save the data and log it
	topo = (bfs_device_topo_t *)buf.getBuffer();
	deviceID = topo->did;
	numBlocks = topo->nblks;
	logMessage(DEVICE_LOG_LEVEL,
			   "Remote device connected (device %lu, %lu blocks).", deviceID,
			   numBlocks);

	// Return successfully
	return (0);
}

/**
 * @brief De-initialze the device
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::bfsDeviceUninitialize(void) {

	// Cleanup the server socket (close connection, free memory)
	if (remoteConn != NULL) {
		remoteConn->disconnect();
		delete remoteConn;
		remoteConn = NULL;
	}

	// Return successfully
	logMessage(DEVICE_LOG_LEVEL, "Remote device disconnected (%lu).", deviceID);
	return (0);
}

/**
 * @brief Submit an io request to the disk worker (implements block IO
 * transaction semantics). Single threaded for now; so the mux coordinates
 * access between file-worker and disk-worker. Allows both sync and async io
 * submission; where later async may benefit certain server-side optimizations,
 * but we will need to begin attaching request IDs to route them correctly back
 * to file I/Os (ie the related block IO fragements); and implement transaction
 * semantics at file IO level. But for now, we normally need sync io submission
 * on client file I/O requests (ie the related block IOs) to ensure
 * linearizability.
 *
 * @param pblk: marshalled request packet containing either outbound request or
 * inbound response message
 * @param sync: flag indicating whether to do a sync or async request
 * @return int BFS_SUCCESS if success, BFS_FAILURE otherwise
 */
int bfsRemoteDevice::submit_io(PBfsBlock &pblk, bool sync) {
	int resp_ready = 0; // 1 OK, 0 WAITING, -1 FAILURE
	(void)sync;

	// For single- and multi-threaded, handle the request directly in the
	// current thread
	bfsConnectionList ready;

	if ((size_t)remoteConn->sendPacketizedBuffer(pblk) != pblk.getLength()) {
		logMessage(LOG_ERROR_LEVEL, "Get block request send failed, error.");
		return (-1);
	}

	// Receive the response packet (quasi-blocking)
	if (remoteMux->waitConnections(ready, 0) ||
		(ready.find(remoteConn->getSocket()) == ready.end()) ||
		(remoteConn->recvPacketizedBuffer(pblk) <= 0)) {
		logMessage(LOG_ERROR_LEVEL, "Error receiving disk response.");
		resp_ready = -1; // mark bio as failed
	} else {
		resp_ready = 1;
	}

	// 1 OK, 0 WAITING, -1 FAILURE
	return (resp_ready == 1) ? BFS_SUCCESS : BFS_FAILURE;
}

/**
 * @brief Get a block from the device
 *
 * @param blkid - the block ID for the block to get (at block id)
 * @param buf - buffer to copy contents into
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::getBlock(PBfsBlock &pblk) {

	// Create the request for the remote device information
	bfs_uid_t usr = 1;
	bfs_device_id_t did = 0;
	bfs_device_msg_t cmd;
	bfs_block_id_t rblkid, blkid;
	char bstr[129];
	bool ack;

	blkid = pblk.get_pbid();

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_start_time) != SGX_SUCCESS) ||
	// 			(f_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	pblk.setData((char *)&blkid, sizeof(bfs_block_id_t));

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double marshal_start_time = 0;

	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice getblock setData: %.3f us",
	// 				   f_end_time - f_start_time);

	// 		marshal_start_time = f_end_time; // set the marshal start time
	// 	}
	// #endif

	logMessage(DEVICE_VRBLOG_LEVEL, "Starting getBlock [%d]", blkid);

	// Marshal the packet
	if (bfsDeviceLayer::marshalBfsDevicePacket(
			usr, deviceID, BFS_DEVICE_GET_BLOCK, 0, secContext, rd_send_seq,
			pblk) == -1) {
		logMessage(LOG_ERROR_LEVEL, "Get block request marhsal failed, error.");
		return (-1);
	}
	rd_send_seq++;

	// Send the pkt
	if (submit_io(pblk) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get block request send/recv failed, error.");
		return (-1);
	}

	// // Send the pkt
	// if ((size_t)remoteConn->sendPacketizedBuffer(pblk) != pblk.getLength()) {
	// 	logMessage(LOG_ERROR_LEVEL, "Get block request send failed, error.");
	// 	return (-1);
	// }

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double unmarshal_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&unmarshal_start_time) != SGX_SUCCESS) ||
	// 			(unmarshal_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// Unmarshal the data, sanity check it
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, pblk) == -1) ||
		(usr != 1) || (did != deviceID) || (cmd != BFS_DEVICE_GET_BLOCK) ||
		(ack != 1) || (pblk.getLength() != sizeof(bfs_block_id_t) + BLK_SZ)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get block request bad data response, abort [usr=%lu, "
				   "did=%lu, cmd=%d, ack=%d, len=%d].",
				   usr, did, cmd, ack, pblk.getLength());
		return (-1);
	}
	rd_recv_seq++;

	// Check the put block value
	pblk >> rblkid;
	if (rblkid != blkid) {
		logMessage(LOG_ERROR_LEVEL,
				   "Returned block ID on get block mismatch [%lu != %lu]",
				   blkid, rblkid);
		return (-1);
	}

	// Copy the data into the buffer, log the successful get block
	// memcpy( pblk.getBuffer(), buf.getBuffer(), buf.getLength() ); // QB: dont
	// need since we read directly into pblk
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		bufToString(pblk.getBuffer(), BLK_SZ, bstr, 128);
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Get block [%lu] device [%lu] data [%s].", blkid, did, bstr);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice getblock unmarshal: %.3f us",
	// 				   f_end_time - unmarshal_start_time);
	// 	}
	// #endif

	logMessage(DEVICE_VRBLOG_LEVEL, "getBlock [%d] success", blkid);

	// Return successfully
	return (0);
}

int bfsRemoteDevice::getBlock(bfs_block_id_t pbid, char *blk) {

	// Create the request for the remote device information
	bfs_uid_t usr = 1;
	bfs_device_id_t did = 0;
	bfs_device_msg_t cmd;
	bfs_block_id_t rblkid;
	char bstr[129];
	bool ack;
	PBfsBlock pblk(NULL, BLK_SZ, 0, 0, 0, 0);

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_start_time) != SGX_SUCCESS) ||
	// 			(f_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	pblk.setData((char *)&pbid, sizeof(bfs_block_id_t));

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double marshal_start_time = 0;

	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice getblock setData: %.3f us",
	// 				   f_end_time - f_start_time);

	// 		marshal_start_time = f_end_time; // set the marshal start time
	// 	}
	// #endif

	logMessage(DEVICE_VRBLOG_LEVEL, "Starting getBlock [%d]", pbid);

	// Marshal the packet
	if (bfsDeviceLayer::marshalBfsDevicePacket(
			usr, deviceID, BFS_DEVICE_GET_BLOCK, 0, secContext, rd_send_seq,
			pblk) == -1) {
		logMessage(LOG_ERROR_LEVEL, "Get block request marhsal failed, error.");
		return (-1);
	}
	rd_send_seq++;

	// Send the pkt
	if (submit_io(pblk) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get block request send/recv failed, error.");
		return (-1);
	}

	// // Send the pkt
	// if ((size_t)remoteConn->sendPacketizedBuffer(pblk) != pblk.getLength()) {
	// 	logMessage(LOG_ERROR_LEVEL, "Get block request send failed, error.");
	// 	return (-1);
	// }

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double unmarshal_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&unmarshal_start_time) != SGX_SUCCESS) ||
	// 			(unmarshal_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// Unmarshal the data, sanity check it
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, pblk) == -1) ||
		(usr != 1) || (did != deviceID) || (cmd != BFS_DEVICE_GET_BLOCK) ||
		(ack != 1) || (pblk.getLength() != sizeof(bfs_block_id_t) + BLK_SZ)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get block request bad data response, abort [usr=%lu, "
				   "did=%lu, cmd=%d, ack=%d, len=%d].",
				   usr, did, cmd, ack, pblk.getLength());
		return (-1);
	}
	rd_recv_seq++;

	// Check the put block value
	pblk >> rblkid;
	if (rblkid != pbid) {
		logMessage(LOG_ERROR_LEVEL,
				   "Returned block ID on get block mismatch [%lu != %lu]", pbid,
				   rblkid);
		return (-1);
	}

	// Copy the data into the buffer, log the successful get block
	// memcpy( pblk.getBuffer(), buf.getBuffer(), buf.getLength() ); // QB: dont
	// need since we read directly into pblk
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		bufToString(pblk.getBuffer(), BLK_SZ, bstr, 128);
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Get block [%lu] device [%lu] data [%s].", pbid, did, bstr);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice getblock unmarshal: %.3f us",
	// 				   f_end_time - unmarshal_start_time);
	// 	}
	// #endif

	// finally copy over the data
	// TODO: deal with buffer management better to avoid unnecessary copies
	memcpy(blk, pblk.getBuffer(), pblk.getLength()); // BLK_SZ copy
	logMessage(DEVICE_VRBLOG_LEVEL, "getBlock [%d] success", pbid);

	// Return successfully
	return (0);
}

/**
 * @brief Put a block into the device
 *
 * @param blkid - the block ID for the block to put (at block id)
 * @param buf - buffer to copy contents into (NULL no copy)
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::putBlock(PBfsBlock &pblk) {

	// Create the request for the remote device information
	bfs_uid_t usr = 1;
	bfs_device_id_t did = deviceID;
	bfs_device_msg_t cmd;
	bfs_block_id_t rblkid, blkid;
	bfsConnectionList ready;
	char bstr[129];
	bool ack;

	// Setup the buffer data block for sending
	blkid = pblk.get_pbid();

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_start_time) != SGX_SUCCESS) ||
	// 			(f_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// buf.setData( pblk.getBuffer(), BLK_SZ );
	pblk << blkid;

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double marshal_start_time = 0;

	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice putblock setData: %.3f us",
	// 				   f_end_time - f_start_time);

	// 		marshal_start_time = f_end_time; // set the marshal start time
	// 	}
	// #endif

	logMessage(DEVICE_VRBLOG_LEVEL, "Starting putBlock [%d]", blkid);

	// Marshal the packet
	if (bfsDeviceLayer::marshalBfsDevicePacket(usr, did, BFS_DEVICE_PUT_BLOCK,
											   0, secContext, rd_send_seq,
											   pblk) == -1) {
		logMessage(LOG_ERROR_LEVEL, "Put block request marshal failed, error.");
		return (-1);
	}
	rd_send_seq++;

	// Send the pkt
	if (submit_io(pblk) != BFS_SUCCESS) {
		logMessage(LOG_ERROR_LEVEL,
				   "Put block request send/recv failed, error.");
		return (-1);
	}

	// // Send the pkt
	// if (remoteConn->sendPacketizedBuffer(pblk) != (int)pblk.getLength()) {
	// 	logMessage(LOG_ERROR_LEVEL, "Put block request send failed, error.");
	// 	return (-1);
	// }

	// #ifdef __BFS_ENCLAVE_MODE
	// 	double unmarshal_start_time = 0.0;
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&unmarshal_start_time) != SGX_SUCCESS) ||
	// 			(unmarshal_start_time == -1))
	// 			return BFS_FAILURE;
	// 	}
	// #endif

	// Unmarshal the data, sanity check it
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, pblk) == -1) ||
		(usr != 1) || (cmd != BFS_DEVICE_PUT_BLOCK) || (ack != 1) ||
		(pblk.getLength() != sizeof(bfs_block_id_t))) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get block request bad data response, abort [usr=%lu, "
				   "did=%lu, cmd=%d, ack=%d, len=%d].",
				   usr, did, cmd, ack, pblk.getLength());
		return (-1);
	}
	rd_recv_seq++;

	// Check the put block value
	pblk >> rblkid;
	if (rblkid != blkid) {
		logMessage(LOG_ERROR_LEVEL,
				   "Returned block ID on put block mismatch [%lu != %lu]",
				   blkid, rblkid);
		return (-1);
	}

	// Log the successful get block
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		bufToString(pblk.getBuffer(), BLK_SZ, bstr, 128);
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Put block [%lu] device [%lu] data [%s].", blkid, did, bstr);
	}

	// #ifdef __BFS_ENCLAVE_MODE
	// 	if (bfsUtilLayer::perf_test() && collect_core_lats) {
	// 		if ((ocall_get_time2(&f_end_time) != SGX_SUCCESS) || (f_end_time ==
	// -1)) 			return BFS_FAILURE;

	// 		logMessage(DEVICE_VRBLOG_LEVEL,
	// 				   "===== Time in remoteDevice putblock unmarshal: %.3f us",
	// 				   f_end_time - unmarshal_start_time);
	// 	}
	// #endif

	logMessage(DEVICE_VRBLOG_LEVEL, "putBlock [%d] success", blkid);

	// Return successfully
	return (0);
}

/**
 * @brief Get a block from the device
 *
 * @param blkid - the block ID for the block to get (at block id)
 * @param buf - buffer to copy contents into
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::getBlocks(bfs_block_list_t &blks) {

	// Create the request for the remote device information
	bfs_block_list_t::iterator it, rit;
	bfs_blockid_list_t manifest;
	bfs_blockid_list_t::iterator mit;
	bfs_uid_t usr = 1;
	bfs_device_id_t did = 0;
	bfs_device_msg_t cmd;
	bfs_block_id_t rblkid;
	bfsConnectionList ready;
	bfsFlexibleBuffer buf;
	bfs_blockid_list_t rlist;
	size_t sz, rsz, expected_size;
	char bbuf[128];
	bool ack;
	string msg;

	// Send the list of elements to get
	sz = blks.size();
	buf.addTrailer(sz);
	for (it = blks.begin(); it != blks.end(); it++) {
		rblkid = (bfs_block_id_t)it->first;
		buf.addTrailer(rblkid);
		manifest.push_back(rblkid);
	}

	// Sending verbose information
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		msg = "";
		for (it = blks.begin(); it != blks.end(); it++) {
			msg += " : " + to_string(it->first);
		}
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Get blocks sending to device=%lu, %u blocks%s", deviceID,
				   blks.size(), msg.c_str());
	}

	// Marshal the packet
	if ((bfsDeviceLayer::marshalBfsDevicePacket(
			 usr, deviceID, BFS_DEVICE_GET_BLOCKS, 0, secContext, rd_send_seq,
			 buf) == -1) ||
		((size_t)remoteConn->sendPacketizedBuffer(buf) != buf.getLength())) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get blocks request marshal/send failed, error.");
		return (-1);
	}
	rd_send_seq++;

	// Receive the response packet (quasi-blocking)
	if ((remoteMux->waitConnections(ready, 0)) ||
		(ready.find(remoteConn->getSocket()) == ready.end()) ||
		(remoteConn->recvPacketizedBuffer(buf) <= 0)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get blocks request receive failed, error.");
		return (-1);
	}

	// Unmarshal the data, sanity check it
	expected_size = sizeof(size_t) + ((sizeof(bfs_block_id_t) + BLK_SZ) * sz);
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, buf) == -1) ||
		(usr != 1) || (did != deviceID) || (cmd != BFS_DEVICE_GET_BLOCKS) ||
		(ack != 1) || (buf.getLength() != expected_size)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Get blocks request bad data response, abort [usr=%lu, "
				   "did=%lu, cmd=%d, ack=%d, len=%d].",
				   usr, did, cmd, ack, buf.getLength());
		return (-1);
	}
	rd_recv_seq++;

	// Check the number of blocks received
	buf >> rsz;
	if (rsz != sz) {
		logMessage(LOG_ERROR_LEVEL,
				   "Incorrect number of blocks returned from get %u != %u", rsz,
				   sz);
		return (-1);
	}

	// Now walk the rest of the blocks
	while (!manifest.empty()) {
		// Get the block ID and check it
		buf >> rblkid;
		if ((mit = std::find(manifest.begin(), manifest.end(), rblkid)) ==
			manifest.end()) {
			logMessage(LOG_ERROR_LEVEL,
					   "Incorrect block returned from get blocks [%lu]",
					   rblkid);
			return (-1);
		}

		// Pull the data off the packet, remove from manifest
		buf.removeHeader(blks[rblkid]->getBuffer(), BLK_SZ);
		manifest.erase(mit);
	}

	// Log, possibly list blocks
	if (levelEnabled(DEVICE_LOG_LEVEL)) {
		msg = "";
		for (it = blks.begin(); it != blks.end(); it++) {
			bufToString(it->second->getBuffer(), 2, bbuf, 128);
			msg += " : " + to_string(it->first) + " (" + bbuf + ")";
		}
	}
	logMessage(DEVICE_LOG_LEVEL, "Get blocks sent to device %lu, %u blocks%s",
			   deviceID, blks.size(), msg.c_str());

	// Return successfully
	return (0);
}

/**
 * @brief Get the blocks associated with the IDS
 *
 * @param blkid - the block ID for the block to put (at block id)
 * @param buf - buffer to copy contents into (NULL no copy)
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::putBlocks(bfs_block_list_t &blks) {

	// Local variables
	bfs_block_list_t::iterator it;
	bfs_blockid_list_t manifest;
	bfs_blockid_list_t::iterator mit;
	bfs_uid_t usr = 1;
	bfs_device_id_t did = deviceID;
	bfs_device_msg_t cmd;
	bfs_block_id_t rblkid;
	bfsConnectionList ready;
	bfsFlexibleBuffer buf;
	size_t sz, rsz, expected_size;
	char bbuf[128];
	bool ack;
	string msg;

	// Send the blocks of data to send
	sz = blks.size();
	buf.addTrailer(sz);
	for (it = blks.begin(); it != blks.end(); it++) {
		rblkid = (bfs_block_id_t)it->first;
		buf.addTrailer(rblkid);
		buf.addTrailer(blks[rblkid]->getBuffer(), BLK_SZ);
		manifest.push_back(rblkid);
	}

	// Sending verbose information
	if (levelEnabled(DEVICE_VRBLOG_LEVEL)) {
		for (it = blks.begin(); it != blks.end(); it++) {
			msg += " : " + to_string(it->first);
		}
		logMessage(DEVICE_VRBLOG_LEVEL,
				   "Put blocks sending to device=%lu, %u blocks%s", deviceID,
				   blks.size(), msg.c_str());
	}

	// Marshal the packet
	if ((bfsDeviceLayer::marshalBfsDevicePacket(usr, did, BFS_DEVICE_PUT_BLOCKS,
												0, secContext, rd_send_seq,
												buf) == -1) ||
		(remoteConn->sendPacketizedBuffer(buf) != (int)buf.getLength())) {
		logMessage(LOG_ERROR_LEVEL, "Put blocks request send failed, error.");
		return (-1);
	}
	rd_send_seq++;

	// Receive the response packet (quasi-blocking)
	if ((remoteMux->waitConnections(ready, 0)) ||
		(ready.find(remoteConn->getSocket()) == ready.end()) ||
		(remoteConn->recvPacketizedBuffer(buf) <= 0)) {
		logMessage(LOG_ERROR_LEVEL, "Put block request receive failed, error.");
		return (-1);
	}

	// Unmarshal the data, sanity check it
	expected_size = sizeof(size_t) + (sizeof(bfs_block_id_t) * sz);
	if ((bfsDeviceLayer::unmarshalBfsDevicePacket(
			 usr, did, cmd, ack, secContext, rd_recv_seq, buf) == -1) ||
		(usr != 1) || (cmd != BFS_DEVICE_PUT_BLOCKS) || (ack != 1) ||
		(buf.getLength() != expected_size)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Put blocks request bad data response [usr=%lu, did=%lu, "
				   "cmd=%d, ack=%d, len=%d].",
				   usr, did, cmd, ack, buf.getLength());
		return (-1);
	}
	rd_recv_seq++;

	// Check the number of blocks received
	buf >> rsz;
	if (rsz != sz) {
		logMessage(
			LOG_ERROR_LEVEL,
			"Incorrect number of blocks returned from put blocks %u != %u", rsz,
			sz);
		return (-1);
	}

	// Now walk the rest of the blocks
	while (!manifest.empty()) {
		// Get the block ID and check it
		buf >> rblkid;
		if ((mit = std::find(manifest.begin(), manifest.end(), rblkid)) ==
			manifest.end()) {
			logMessage(LOG_ERROR_LEVEL,
					   "Incorrect block returned from get blocks [%lu]",
					   rblkid);
			return (-1);
		}
		manifest.erase(mit);
	}

	// Log, possibly list blocks
	if (levelEnabled(DEVICE_LOG_LEVEL)) {
		msg = "";
		for (it = blks.begin(); it != blks.end(); it++) {
			bufToString(it->second->getBuffer(), 2, bbuf, 128);
			msg += " : " + to_string(it->first) + " (" + bbuf + ")";
		}
	}
	logMessage(DEVICE_LOG_LEVEL, "Put blocks sent to device %lu, %u blocks%s",
			   deviceID, blks.size(), msg.c_str());

	// Return successfully
	return (0);
}

// Static class methods

//
// Private class functions

/**
 * @brief The default constructor for the class (private)
 *
 * @param none
 * @return int : 0 is success, -1 is failure
 */

bfsRemoteDevice::bfsRemoteDevice(void) {

	// Return, no return code
	return;
}

/**
 * @brief Change the state of the device
 *
 * @param st - the state to change the device into
 * @return int : 0 is success, -1 is failure
 */

int bfsRemoteDevice::changeDeviceState(bfs_device_state_t st) {

	// Sanity check the new state
	if ((st < BFSDEV_UNINITIALIZED) || (st > BFSDEV_UKNOWN)) {
		logMessage(LOG_ERROR_LEVEL,
				   "Trying change device state to nonsense value [%u]", st);
		st = BFSDEV_ERRORED;
		return (-1);
	}

	// Just log and change the state of the device
	logMessage(DEVICE_LOG_LEVEL, "Change device [%lu] state from [%s] to [%s]",
			   deviceID, bfsDeviceLayer::getDeviceStateStr(devState),
			   bfsDeviceLayer::getDeviceStateStr(st));
	devState = st;

	// Return successfully
	return (0);
}
