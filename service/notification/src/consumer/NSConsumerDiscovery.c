//******************************************************************
//
// Copyright 2016 Samsung Electronics All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "NSConsumerDiscovery.h"

#include <string.h>
#include "NSCommon.h"
#include "NSConsumerCommon.h"
#include "NSConstants.h"
#include "ocpayload.h"
#include "oic_malloc.h"
#include "oic_string.h"

#define NS_DISCOVER_QUERY "/oic/res?rt=oic.r.notification"
#define NS_PRESENCE_SUBSCRIBE_QUERY_TCP "/oic/ad?rt=oic.r.notification"
#define NS_GET_INFORMATION_QUERY "/notification?if=oic.if.notification"

NSProvider_internal * NSGetProvider(OCClientResponse * clientResponse);

OCDevAddr * NSChangeAddress(const char * address);

OCStackApplicationResult NSConsumerPresenceListener(
        void * ctx, OCDoHandle handle, OCClientResponse * clientResponse)
{
    (void) ctx;
    (void) handle;

    NS_VERIFY_NOT_NULL(clientResponse, OC_STACK_KEEP_TRANSACTION);
    NS_VERIFY_STACK_SUCCESS(
            NSOCResultToSuccess(clientResponse->result), OC_STACK_KEEP_TRANSACTION);

    NS_LOG_V(DEBUG, "Presence income : %s:%d",
            clientResponse->devAddr.addr, clientResponse->devAddr.port);
    NS_LOG_V(DEBUG, "Presence result : %d",
            clientResponse->result);
    NS_LOG_V(DEBUG, "Presence sequenceNum : %d",
            clientResponse->sequenceNumber);
    NS_LOG_V(DEBUG, "Presence Transport Type : %d",
                clientResponse->devAddr.adapter);

    if (!NSIsStartedConsumer())
    {
        return OC_STACK_DELETE_TRANSACTION;
    }

    OCPresencePayload * payload = (OCPresencePayload *)clientResponse->payload;
    if (payload->trigger == OC_PRESENCE_TRIGGER_DELETE ||
            clientResponse->result == OC_STACK_PRESENCE_STOPPED)
    {
        NS_LOG(DEBUG, "stopped presence or resource is deleted.");
        NS_LOG(DEBUG, "build NSTask");
        OCDevAddr * addr = (OCDevAddr *)OICMalloc(sizeof(OCDevAddr));
        NS_VERIFY_NOT_NULL(addr, OC_STACK_KEEP_TRANSACTION);
        memcpy(addr, clientResponse->addr, sizeof(OCDevAddr));

        NSTask * task = NSMakeTask(TASK_CONSUMER_PROVIDER_DELETED, addr);
        NS_VERIFY_NOT_NULL(task, OC_STACK_KEEP_TRANSACTION);

        NSConsumerPushEvent(task);
    }

    else if (payload->trigger == OC_PRESENCE_TRIGGER_CREATE)
    {
        NS_LOG(DEBUG, "started presence or resource is created.");
        NSInvokeRequest(NULL, OC_REST_DISCOVER, clientResponse->addr,
            NS_DISCOVER_QUERY, NULL, NSProviderDiscoverListener, NULL,
            clientResponse->addr->adapter);
    }

    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult NSProviderDiscoverListener(
        void * ctx, OCDoHandle handle, OCClientResponse * clientResponse)
{
    (void) handle;

    NS_VERIFY_NOT_NULL(clientResponse, OC_STACK_KEEP_TRANSACTION);
    NS_VERIFY_NOT_NULL(clientResponse->payload, OC_STACK_KEEP_TRANSACTION);
    NS_VERIFY_STACK_SUCCESS(NSOCResultToSuccess(clientResponse->result), OC_STACK_KEEP_TRANSACTION);

    NS_LOG_V(DEBUG, "Discover income : %s:%d",
            clientResponse->devAddr.addr, clientResponse->devAddr.port);
    NS_LOG_V(DEBUG, "Discover result : %d",
            clientResponse->result);
    NS_LOG_V(DEBUG, "Discover sequenceNum : %d",
            clientResponse->sequenceNumber);
    NS_LOG_V(DEBUG, "Discover Transport Type : %d",
                    clientResponse->devAddr.adapter);

    if (!NSIsStartedConsumer())
    {
        return OC_STACK_DELETE_TRANSACTION;
    }

    OCResourcePayload * resource = ((OCDiscoveryPayload *)clientResponse->payload)->resources;
    while (resource)
    {
        NS_VERIFY_NOT_NULL(resource->uri, OC_STACK_KEEP_TRANSACTION);
        if (strstr(resource->uri, NS_RESOURCE_URI))
        {
            OCConnectivityType type = CT_DEFAULT;
            if (clientResponse->addr->adapter == OC_ADAPTER_TCP)
            {
                type = CT_ADAPTER_TCP;
            }

            NSInvokeRequest(NULL, OC_REST_GET, clientResponse->addr,
                    resource->uri, NULL, NSIntrospectProvider, ctx,
                    type);
        }
        resource = resource->next;
    }

    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult NSIntrospectProvider(
        void * ctx, OCDoHandle handle, OCClientResponse * clientResponse)
{
    (void) handle;

    NS_VERIFY_NOT_NULL(clientResponse, OC_STACK_KEEP_TRANSACTION);
    NS_VERIFY_STACK_SUCCESS(NSOCResultToSuccess(clientResponse->result), OC_STACK_KEEP_TRANSACTION);

    NS_LOG_V(DEBUG, "GET response income : %s:%d",
            clientResponse->devAddr.addr, clientResponse->devAddr.port);
    NS_LOG_V(DEBUG, "GET response result : %d",
            clientResponse->result);
    NS_LOG_V(DEBUG, "GET response sequenceNum : %d",
            clientResponse->sequenceNumber);
    NS_LOG_V(DEBUG, "GET response resource uri : %s",
            clientResponse->resourceUri);
    NS_LOG_V(DEBUG, "GET response Transport Type : %d",
                    clientResponse->devAddr.adapter);

    if (!NSIsStartedConsumer())
    {
        return OC_STACK_DELETE_TRANSACTION;
    }

    NSProvider_internal * newProvider = NSGetProvider(clientResponse);
    NS_VERIFY_NOT_NULL(newProvider, OC_STACK_KEEP_TRANSACTION);
    if (ctx && *((NSConsumerDiscoverType *)ctx) == NS_DISCOVER_CLOUD )
    {
        newProvider->connection->isCloudConnection = true;
        NSOICFree(ctx);
    }

    NS_LOG(DEBUG, "build NSTask");
    NSTask * task = NSMakeTask(TASK_CONSUMER_PROVIDER_DISCOVERED, (void *) newProvider);
    NS_VERIFY_NOT_NULL_WITH_POST_CLEANING(task, NS_ERROR, NSRemoveProvider_internal(newProvider));

    NSConsumerPushEvent(task);

    return OC_STACK_KEEP_TRANSACTION;
}

void NSGetProviderPostClean(
        char * pId, char * mUri, char * sUri, char * tUri, NSProviderConnectionInfo * connection)
{
    NSOICFree(pId);
    NSOICFree(mUri);
    NSOICFree(sUri);
    NSOICFree(tUri);
    NSRemoveConnections(connection);
}

NSProvider_internal * NSGetProvider(OCClientResponse * clientResponse)
{
    NS_LOG(DEBUG, "create NSProvider");
    NS_VERIFY_NOT_NULL(clientResponse->payload, NULL);

    OCRepPayload * payload = (OCRepPayload *)clientResponse->payload;
    while (payload)
    {
        NS_LOG_V(DEBUG, "Payload Key : %s", payload->values->name);
        payload = payload->next;
    }

    payload = (OCRepPayload *)clientResponse->payload;

    char * providerId = NULL;
    char * messageUri = NULL;
    char * syncUri = NULL;
    char * topicUri = NULL;
    int64_t accepter = 0;
    NSProviderConnectionInfo * connection = NULL;

    NS_LOG(DEBUG, "get information of accepter");
    bool getResult = OCRepPayloadGetPropInt(payload, NS_ATTRIBUTE_POLICY, & accepter);
    NS_VERIFY_NOT_NULL(getResult == true ? (void *) 1 : NULL, NULL);

    NS_LOG(DEBUG, "get provider ID");
    getResult = OCRepPayloadGetPropString(payload, NS_ATTRIBUTE_PROVIDER_ID, & providerId);
    NS_VERIFY_NOT_NULL(getResult == true ? (void *) 1 : NULL, NULL);

    NS_LOG(DEBUG, "get message URI");
    getResult = OCRepPayloadGetPropString(payload, NS_ATTRIBUTE_MESSAGE, & messageUri);
    NS_VERIFY_NOT_NULL_WITH_POST_CLEANING(getResult == true ? (void *) 1 : NULL, NULL,
            NSGetProviderPostClean(providerId, messageUri, syncUri, topicUri, connection));

    NS_LOG(DEBUG, "get sync URI");
    getResult = OCRepPayloadGetPropString(payload, NS_ATTRIBUTE_SYNC, & syncUri);
    NS_VERIFY_NOT_NULL_WITH_POST_CLEANING(getResult == true ? (void *) 1 : NULL, NULL,
            NSGetProviderPostClean(providerId, messageUri, syncUri, topicUri, connection));

    NS_LOG(DEBUG, "get topic URI");
    getResult = OCRepPayloadGetPropString(payload, NS_ATTRIBUTE_TOPIC, & topicUri);

    NS_LOG(DEBUG, "get provider connection information");
    NS_VERIFY_NOT_NULL(clientResponse->addr, NULL);
    connection = NSCreateProviderConnections(clientResponse->addr);
    NS_VERIFY_NOT_NULL(connection, NULL);

    NSProvider_internal * newProvider
        = (NSProvider_internal *)OICMalloc(sizeof(NSProvider_internal));
    NS_VERIFY_NOT_NULL_WITH_POST_CLEANING(newProvider, NULL,
          NSGetProviderPostClean(providerId, messageUri, syncUri, topicUri, connection));

    OICStrcpy(newProvider->providerId, sizeof(char) * NS_DEVICE_ID_LENGTH, providerId);
    NSOICFree(providerId);
    newProvider->messageUri = messageUri;
    newProvider->syncUri = syncUri;
    newProvider->topicUri = NULL;
    if (topicUri && strlen(topicUri) > 0)
    {
        newProvider->topicUri = topicUri;
    }
    newProvider->accessPolicy = (NSSelector)accepter;
    newProvider->connection = connection;
    newProvider->topicLL = NULL;

    return newProvider;
}

OCDevAddr * NSChangeAddress(const char * address)
{
    NS_VERIFY_NOT_NULL(address, NULL);
    OCDevAddr * retAddr = NULL;

    int index = 0;
    while(address[index] != '\0')
    {
        if (address[index] == ':')
        {
            break;
        }
        index++;
    }

    if (address[index] == '\0')
    {
        return NULL;
    }

    int tmp = index + 1;
    uint16_t port = address[tmp++];

    while(address[tmp] != '\0')
    {
        port *= 10;
        port += address[tmp++] - '0';
    }

    retAddr = (OCDevAddr *) OICMalloc(sizeof(OCDevAddr));
    NS_VERIFY_NOT_NULL(retAddr, NULL);

    retAddr->adapter = OC_ADAPTER_TCP;
    OICStrcpy(retAddr->addr, index - 1, address);
    retAddr->addr[index] = '\0';
    retAddr->port = port;

    return retAddr;
}

void NSConsumerHandleRequestDiscover(OCDevAddr * address, NSConsumerDiscoverType rType)
{
    OCConnectivityType type = CT_DEFAULT;
    NSConsumerDiscoverType * callbackData = NULL;

    if (address)
    {
        if (address->adapter == OC_ADAPTER_IP)
        {
            type = CT_ADAPTER_IP;
            NS_LOG(DEBUG, "Request discover [UDP]");
        }
        else if (address->adapter == OC_ADAPTER_TCP)
        {
            type = CT_ADAPTER_TCP;
            NS_LOG(DEBUG, "Request discover and subscribe presence [TCP]");
            NS_LOG(DEBUG, "Subscribe presence [TCP]");
            NSInvokeRequest(NULL, OC_REST_PRESENCE, address, NS_PRESENCE_SUBSCRIBE_QUERY_TCP,
                    NULL, NSConsumerPresenceListener, NULL, type);

            if (rType == NS_DISCOVER_CLOUD)
            {
                callbackData = (NSConsumerDiscoverType *)OICMalloc(sizeof(NSConsumerDiscoverType));
                *callbackData = NS_DISCOVER_CLOUD;
            }
        }
        else
        {
            NS_LOG_V(DEBUG, "Request discover But Adapter is not IP : %d", address->adapter);
        }
    }
    else
    {
        NS_LOG(DEBUG, "Request Multicast discover [UDP]");
    }

    NSInvokeRequest(NULL, OC_REST_DISCOVER, address, NS_DISCOVER_QUERY,
            NULL, NSProviderDiscoverListener, (void *)callbackData, type);
}

void NSConsumerDiscoveryTaskProcessing(NSTask * task)
{
    NS_VERIFY_NOT_NULL_V(task);

    NS_LOG_V(DEBUG, "Receive Event : %d", (int)task->taskType);
    if (task->taskType == TASK_CONSUMER_REQ_DISCOVER)
    {
        char * address = (char *) task->taskData;
        NSConsumerDiscoverType dType = NS_DISCOVER_DEFAULT;

        OCDevAddr * addr = NULL;
        if (address)
        {
            addr = NSChangeAddress(address);
            dType = NS_DISCOVER_CLOUD;
        }

        NSConsumerHandleRequestDiscover(addr, dType);
        NSOICFree(task->taskData);
        NSOICFree(addr);
    }
    else if (task->taskType == TASK_EVENT_CONNECTED || task->taskType == TASK_EVENT_CONNECTED_TCP)
    {
        NSConsumerHandleRequestDiscover((OCDevAddr *) task->taskData, NS_DISCOVER_DEFAULT);
        NSOICFree(task->taskData);
    }
    else
    {
        NS_LOG(ERROR, "Unknown type message");
    }

    NSOICFree(task);
}
