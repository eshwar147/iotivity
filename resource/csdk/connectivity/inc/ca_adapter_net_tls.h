/* *****************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************/
#ifndef CA_ADAPTER_NET_TLS_H_
#define CA_ADAPTER_NET_TLS_H_

#include "caadapterutils.h"
#include "cainterface.h"

/**
 * Currently TLS supported adapters(2) WIFI and ETHENET for linux platform.
 */
#define MAX_SUPPORTED_ADAPTERS 2

typedef void (*CAPacketReceivedCallback)(const CASecureEndpoint_t *sep,
                                         const void *data, uint32_t dataLength);

typedef void (*CAPacketSendCallback)(CAEndpoint_t *endpoint,
                                         const void *data, uint32_t dataLength);

/**
 * Select the cipher suite for dtls handshake
 *
 * @param[in] cipher    cipher suite
 *                             0xC018 : TLS_ECDH_anon_WITH_AES_128_CBC_SHA_256
 *                             0xC0A8 : TLS_PSK_WITH_AES_128_CCM_8
 *                             0xC0AE : TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8
 *
 * @retval  ::CA_STATUS_OK for success, otherwise some error value
 */
CAResult_t CAsetTlsCipherSuite(const uint32_t cipher);

/**
 * Used set send and recv callbacks for different adapters(WIFI,EtherNet).
 *
 * @param[in]  recvCallback    packet received callback.
 * @param[in]  sendCallback    packet sent callback.
 * @param[in]  type  type of adapter.
 *
 */
void CAsetTlsAdapterCallbacks(CAPacketReceivedCallback recvCallback,
                              CAPacketSendCallback sendCallback,
                              CATransportAdapter_t type);

/**
 * Register callback to get TLS PSK credentials.
 * @param[in]  credCallback    callback to get TLS PSK credentials.
 */
void CAsetTlsCredentialsCallback(CAGetDTLSPskCredentialsHandler credCallback);

/**
 * Close the TLS session
 *
 * @param[in] endpoint  information of network address
 *
 * @retval  ::CA_STATUS_OK for success, otherwise some error value
 */
CAResult_t CAcloseTlsConnection(const CAEndpoint_t *endpoint);

/**
 * initialize mbedTLS library and other necessary initialization.
 *
 * @return  0 on success otherwise a positive error value.
 * @retval  ::CA_STATUS_OK  Successful.
 * @retval  ::CA_MEMORY_ALLOC_FAILED  Memory allocation failed.
 * @retval  ::CA_STATUS_FAILED Operation failed.
 *
 */
CAResult_t CAinitTlsAdapter();

/**
 * de-inits mbedTLS library and free the allocated memory.
 */
void CAdeinitTlsAdapter();

/**
 * Performs TLS encryption of the CoAP PDU.
 *
 * If a DTLS session does not exist yet with the @dst,
 * a TLS handshake will be started. In case where a new TLS handshake
 * is started, pdu info is cached to be send when session setup is finished.
 *
 * @param[in]  endpoint  address to which data will be sent.
 * @param[in]  data  length of data.
 * @param[in]  dataLen  length of given data
 *
 * @return  0 on success otherwise a positive error value.
 * @retval  ::CA_STATUS_OK  Successful.
 * @retval  ::CA_STATUS_INVALID_PARAM  Invalid input arguments.
 * @retval  ::CA_STATUS_FAILED Operation failed.
 *
 */

CAResult_t CAencryptTls(const CAEndpoint_t *endpoint, void *data, uint32_t dataLen);

/**
 * Performs TLS decryption of the data.
 *
 * @param[in]  sep  address and flags for which data will be decrypted.
 * @param[in]  data  length of data.
 * @param[in]  dataLen  length of given data
 *
 * @return  0 on success otherwise a positive error value.
 * @retval  ::CA_STATUS_OK  Successful.
 * @retval  ::CA_STATUS_INVALID_PARAM  Invalid input arguments.
 * @retval  ::CA_STATUS_FAILED Operation failed.
 *
 */
CAResult_t CAdecryptTls(const CASecureEndpoint_t *sep, uint8_t *data, uint32_t dataLen);

/**
 * Initiate TLS handshake with selected cipher suite.
 *
 * @param[in] endpoint  information of network address
 *
 * @retval  ::CA_STATUS_OK for success, otherwise some error value
 */
CAResult_t CAinitiateTlsHandshake(const CAEndpoint_t *endpoint);

/**
 * Register callback to deliver the result of TLS handshake
 * @param[in] tlsHandshakeCallback Callback to receive the result of TLS handshake.
 */
void CAsetTlsHandshakeCallback(CAErrorCallback tlsHandshakeCallback);

/**
 * Generate ownerPSK using the PKCS#12 derivation function
 *
 * @param[in,out] ownerPSK  Output buffer for owner PSK
 * @param[in] ownerPSKSize  Byte length of the ownerPSK to be generated
 * @param[in] deviceID  ID of new device(Resource Server)
 * @param[in] deviceIDLen  Byte length of deviceID
 *
 * @retval  ::CA_STATUS_OK for success, otherwise some error value
 */
CAResult_t CAtlsGenerateOwnerPSK(const CAEndpoint_t *endpoint,
                                 uint8_t* ownerPSK, const size_t ownerPSKSize,
                                 const uint8_t* deviceID, const size_t deviceIDLen);

#endif /* CA_ADAPTER_NET_TLS_H_ */


