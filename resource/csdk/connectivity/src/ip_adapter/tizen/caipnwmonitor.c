/******************************************************************
*
* Copyright 2014 Samsung Electronics All Rights Reserved.
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

#include "caipinterface.h"

#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <wifi.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "caadapterutils.h"
#include "logger.h"
#include "oic_malloc.h"
#include "oic_string.h"

#define TAG "IP_MONITOR"

#define MAX_INTERFACE_INFO_LENGTH (1024)

#define NETLINK_MESSAGE_LENGTH  (4096)

static CAIPConnectionStateChangeCallback g_networkChangeCallback;

static CAInterface_t *CANewInterfaceItem(int index, char *name, int family,
                                         const char *addr, int flags);

static CAResult_t CAAddInterfaceItem(u_arraylist_t *iflist, int index,
                                     char *name, int family, const char *addr, int flags);

static void CAWIFIConnectionStateChangedCb(wifi_connection_state_e state, wifi_ap_h ap,
                                           void *userData);

static void CAWIFIDeviceStateChangedCb(wifi_device_state_e state, void *userData);


int CAGetPollingInterval(int interval)
{
    return interval;
}

void CAIPSetNetworkMonitorCallback(CAIPConnectionStateChangeCallback callback)
{
    g_networkChangeCallback = callback;
}

CAInterface_t *CAFindInterfaceChange()
{
    CAInterface_t *foundNewInterface = NULL;
    char buf[NETLINK_MESSAGE_LENGTH] = { 0 };
    struct sockaddr_nl sa = { 0 };
    struct iovec iov = { .iov_base = buf,
                         .iov_len = sizeof (buf) };
    struct msghdr msg = { .msg_name = (void *)&sa,
                          .msg_namelen = sizeof (sa),
                          .msg_iov = &iov,
                          .msg_iovlen = 1 };

    size_t len = recvmsg(caglobals.ip.netlinkFd, &msg, 0);

    for (struct nlmsghdr *nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len))
    {
        if (nh != NULL && nh->nlmsg_type != RTM_NEWLINK)
        {
            continue;
        }

        struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(nh);

        int ifiIndex = ifi->ifi_index;
        u_arraylist_t *iflist = CAIPGetInterfaceInformation(ifiIndex);

        if ((!ifi || (ifi->ifi_flags & IFF_LOOPBACK) || !(ifi->ifi_flags & IFF_RUNNING)))
        {
            continue;
        }

        if (!iflist)
        {
            OIC_LOG_V(ERROR, TAG, "get interface info failed: %s", strerror(errno));
            return NULL;
        }

        uint32_t listLength = u_arraylist_length(iflist);
        for (uint32_t i = 0; i < listLength; i++)
        {
            CAInterface_t *ifitem = (CAInterface_t *)u_arraylist_get(iflist, i);
            if (!ifitem)
            {
                continue;
            }

            if ((int)ifitem->index != ifiIndex)
            {
                continue;
            }

            foundNewInterface = CANewInterfaceItem(ifitem->index, ifitem->name, ifitem->family,
                                                   ifitem->addr, ifitem->flags);
            break;    // we found the one we were looking for
        }
        u_arraylist_destroy(iflist);
    }
    return foundNewInterface;
}

CAResult_t CAIPStartNetworkMonitor()
{
    OIC_LOG(DEBUG, TAG, "IN");

     // Initialize Wifi service
    wifi_error_e ret = wifi_initialize();
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_initialize failed");
        return CA_STATUS_FAILED;
    }

    // Set callback for receiving state changes
    ret = wifi_set_device_state_changed_cb(CAWIFIDeviceStateChangedCb, NULL);
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_set_device_state_changed_cb failed");
        return CA_STATUS_FAILED;
    }

    // Set callback for receiving connection state changes
    ret = wifi_set_connection_state_changed_cb(CAWIFIConnectionStateChangedCb, NULL);
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_set_connection_state_changed_cb failed");
        return CA_STATUS_FAILED;
    }

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

CAResult_t CAIPStopNetworkMonitor()
{
    OIC_LOG(DEBUG, TAG, "IN");

     // Reset callback for receiving state changes
    wifi_error_e ret = wifi_unset_device_state_changed_cb();
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_unset_device_state_changed_cb failed");
    }

    // Reset callback for receiving connection state changes
    ret = wifi_unset_connection_state_changed_cb();
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_unset_connection_state_changed_cb failed");
    }

    // Deinitialize Wifi service
    ret = wifi_deinitialize();
    if (WIFI_ERROR_NONE != ret)
    {
        OIC_LOG(ERROR, TAG, "wifi_deinitialize failed");
    }

    OIC_LOG(DEBUG, TAG, "OUT");
    return CA_STATUS_OK;
}

u_arraylist_t *CAIPGetInterfaceInformation(int desiredIndex)
{
    u_arraylist_t *iflist = u_arraylist_create();
    if (!iflist)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to create iflist: %s", strerror(errno));
        return NULL;
    }

    char buf[MAX_INTERFACE_INFO_LENGTH] = { 0 };
    struct ifconf ifc = { .ifc_len = MAX_INTERFACE_INFO_LENGTH, .ifc_buf = buf };

    int s = caglobals.ip.u6.fd != -1 ? caglobals.ip.u6.fd : caglobals.ip.u4.fd;
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
    {
        OIC_LOG_V(ERROR, TAG, "SIOCGIFCONF failed: %s", strerror(errno));
        u_arraylist_destroy(iflist);
        return NULL;
    }

    struct ifreq* ifr = ifc.ifc_req;
    size_t interfaces = ifc.ifc_len / sizeof (ifc.ifc_req[0]);
    size_t ifreqsize = ifc.ifc_len;

    if (ifreqsize > caglobals.ip.nm.sizeIfItems)
    {
        CAIfItem_t *items = (CAIfItem_t *)OICRealloc(caglobals.ip.nm.ifItems, ifreqsize);
        if (!items)
        {
            OIC_LOG(ERROR, TAG, "OICRealloc failed");
            goto exit;
        }
        caglobals.ip.nm.ifItems = items;
        caglobals.ip.nm.sizeIfItems = ifreqsize;
    }

    caglobals.ip.nm.numIfItems = 0;
    for (size_t i = 0; i < interfaces; i++)
    {
        struct ifreq* item = &ifr[i];
        char *name = item->ifr_name;

        if (ioctl(s, SIOCGIFFLAGS, item) < 0)
        {
            OIC_LOG_V(ERROR, TAG, "SIOCGIFFLAGS failed: %s", strerror(errno));
            continue;
        }
        int16_t flags = item->ifr_flags;
        if ((flags & IFF_LOOPBACK) || !(flags & IFF_RUNNING))
        {
            continue;
        }
        if (ioctl(s, SIOCGIFINDEX, item) < 0)
        {
            OIC_LOG_V(ERROR, TAG, "SIOCGIFINDEX failed: %s", strerror(errno));
            continue;
        }

        int ifindex = item->ifr_ifindex;
        caglobals.ip.nm.ifItems[i].ifIndex = ifindex;
        caglobals.ip.nm.numIfItems++;

        if (desiredIndex && (ifindex != desiredIndex))
        {
            continue;
        }

        // Get address of network interface.
        char addr[MAX_ADDR_STR_SIZE_CA] = { 0 };
        struct sockaddr_in *sa = (struct sockaddr_in *)&item->ifr_addr;
        inet_ntop(AF_INET, (void *)&(sa->sin_addr), addr, sizeof(addr));

        // Add IPv4 interface
        CAResult_t result = CAAddInterfaceItem(iflist, ifindex, name, AF_INET, addr, flags);
        if (CA_STATUS_OK != result)
        {
            goto exit;
        }
    }
    return iflist;

exit:
    u_arraylist_destroy(iflist);
    return NULL;
}

static CAResult_t CAAddInterfaceItem(u_arraylist_t *iflist, int index,
                                     char *name, int family, const char *addr, int flags)
{
    CAInterface_t *ifitem = CANewInterfaceItem(index, name, family, addr, flags);
    if (!ifitem)
    {
        return CA_STATUS_FAILED;
    }
    bool result = u_arraylist_add(iflist, ifitem);
    if (!result)
    {
        OIC_LOG(ERROR, TAG, "u_arraylist_add failed.");
        OICFree(ifitem);
        return CA_STATUS_FAILED;
    }

    return CA_STATUS_OK;
}

static CAInterface_t *CANewInterfaceItem(int index, char *name, int family,
                                         const char *addr, int flags)
{
    CAInterface_t *ifitem = (CAInterface_t *)OICCalloc(1, sizeof (CAInterface_t));
    if (!ifitem)
    {
        OIC_LOG(ERROR, TAG, "Malloc failed");
        return NULL;
    }

    OICStrcpy(ifitem->name, INTERFACE_NAME_MAX, name);
    ifitem->index = index;
    ifitem->family = family;
    OICStrcpy(ifitem->addr, sizeof(ifitem->addr), addr);
    ifitem->flags = flags;

    return ifitem;
}

void CAWIFIConnectionStateChangedCb(wifi_connection_state_e state, wifi_ap_h ap,
                                    void *userData)
{
    OIC_LOG(DEBUG, TAG, "IN");

    if (WIFI_CONNECTION_STATE_ASSOCIATION == state
        || WIFI_CONNECTION_STATE_CONFIGURATION == state)
    {
        OIC_LOG(DEBUG, TAG, "Connection is in Association State");
        return;
    }

    if (WIFI_CONNECTION_STATE_CONNECTED == state)
    {
        g_networkChangeCallback(CA_ADAPTER_IP, CA_INTERFACE_UP);
    }
    else
    {
        g_networkChangeCallback(CA_ADAPTER_IP, CA_INTERFACE_DOWN);
    }

    OIC_LOG(DEBUG, TAG, "OUT");
}

void CAWIFIDeviceStateChangedCb(wifi_device_state_e state, void *userData)
{
    OIC_LOG(DEBUG, TAG, "IN");

    if (WIFI_DEVICE_STATE_ACTIVATED == state)
    {
        OIC_LOG(DEBUG, TAG, "Wifi is in Activated State");
    }
    else
    {
        CAWIFIConnectionStateChangedCb(WIFI_CONNECTION_STATE_DISCONNECTED, NULL, NULL);
        OIC_LOG(DEBUG, TAG, "Wifi is in Deactivated State");
    }

    OIC_LOG(DEBUG, TAG, "OUT");
}
