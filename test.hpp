#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <errno.h>
#include <stdint.h>
#include <netinet/in.h>
#include <net/ethernet.h>
#include <signal.h>
#include <unistd.h> // Just for sleep
#include <errno.h>
#include <fcntl.h>

#include <string>
#include <iostream>
#include "Structs.hpp"

// includes for open62541 stack
#define _GNU_SOURCE

#include <pthread.h>

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/types_generated.h>
#include <open62541/plugin/pubsub_ethernet.h>

#include "../deps/open62541/src/pubsub/ua_pubsub.h"
#include "isgcncDataModel.h"

#define             DEFAULT_CYCLE_TIME                    0.25
/* Qbv offset */
#define             QBV_OFFSET                            25 * 1000
#define             DEFAULT_SOCKET_PRIORITY               3
#define             PUBLISHER_ID                          2234
#define             WRITER_GROUP_ID                       101
#define             DATA_SET_WRITER_ID                    62541
#define             PUBLISHING_MAC_ADDRESS                "opc.eth://C8-B5-76-21-EE-83"
#define             PORT_NUMBER                           68

#define             MILLI_SECONDS                         1000 * 1000
#define             SECONDS                               1000 * 1000 * 1000
#define             SECONDS_SLEEP                         5
#define             DEFAULT_PUB_SCHED_PRIORITY            78
#define             DEFAULT_PUBSUB_CORE                   2
#define             DEFAULT_USER_APP_CORE                 3
#define             MAX_MEASUREMENTS                      30000000
#define             SECONDS_INCREMENT                     1
#define             CLOCKID                               CLOCK_TAI
#define             ETH_TRANSPORT_PROFILE                 "http://opcfoundation.org/UA-Profile/Transport/pubsub-eth-uadp"
#define             DEFAULT_USERAPPLICATION_SCHED_PRIORITY 75


static RawMessage init_message()
{
    RawMessage msg;
    memset(&msg, 0, sizeof(RawMessage));

    msg.header.DestinationMAC.b[0] = 0x00;
    msg.header.DestinationMAC.b[1] = 0x1B;
    msg.header.DestinationMAC.b[2] = 0x21;
    msg.header.DestinationMAC.b[3] = 0xEA;
    msg.header.DestinationMAC.b[4] = 0xB8;
    msg.header.DestinationMAC.b[5] = 0xEA;
    // msg.header.SourceMAC will be set automatically

    // Not used because of stripped Q Header
    // msg.header.QHeader.TPID = htons(0x8100);
    // msg.header.QHeader.VLAN.PCP = 5;
    // msg.header.QHeader.VLAN.DEI = 0;
    // msg.header.QHeader.VLAN.VLANId = 0x000;
    // msg.header.QHeader.VLAN._fields = htons(0xA4D2); // PCP=5, DEI=0, VLANId=1234

    msg.header.EtherType = 0xB62C;

    msg.data.NetworkMessageHeader.VersionFlags.UADPVersion = 1;
    msg.data.NetworkMessageHeader.VersionFlags.PublisherId = 1;
    msg.data.NetworkMessageHeader.VersionFlags.GroupHeader = 1;
    msg.data.NetworkMessageHeader.ExtendedFlags1.Timestamp = 0;
    msg.data.NetworkMessageHeader.ExtendedFlags1.PicoSeconds = 0;
    msg.data.NetworkMessageHeader.ExtendedFlags1.ExtendedFlags2 = 0;

    msg.data.NetworkMessageHeader.PublisherId = 1;
    msg.data.GroupHeader.GroupFlags.WriterGroupId = 1;
    msg.data.GroupHeader.GroupFlags.GroupVersion = 1;
    msg.data.GroupHeader.GroupFlags.NetworkMessageNumber = 1;
    msg.data.GroupHeader.GroupFlags.SequenceNumber = 1;

    msg.data.GroupHeader.WriterGroupId = 1;
    msg.data.GroupHeader.GroupVersion = 1;
    msg.data.GroupHeader.NetworkMessageNumber = 1;
    msg.data.GroupHeader.SequenceNumber = 1;

    msg.data.GroupHeader.GroupFlags.WriterGroupId = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.Valid = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.FieldEncoding = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.DataSetMessageSequenceNumber = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.Status = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.ConfigurationVersionMajorVersion = 0;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.ConfigurationVersionMinorVersion = 0;
    msg.data.DataSetMessage.DataSetMessageHeader.DataSetFlags1.DataSetFlags2 = 0;

    msg.data.DataSetMessage.DataSetMessageHeader.DataSetMessageSequenceNumber = 1;
    msg.data.DataSetMessage.DataSetMessageHeader.StatusCode = 0;

    return msg;
}

static int createEthernetAddrStruct(struct sockaddr_ll *address, RawMessage *msg, uint8_t *mac_address, int sockfd, char *ifName)
{
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    memcpy(ifr.ifr_name, ifName, strnlen(ifName, IFNAMSIZ));
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1)
    {
        return -1;
    }
    else
    {
        memset(address, 0, sizeof(struct sockaddr_ll));
        address->sll_family = AF_PACKET;
        // address->sll_protocol = msg.header.QHeader.TPID;
        // address->sll_protocol = htons(ETH_P_ALL);
        address->sll_protocol = htons(msg->header.EtherType);
        address->sll_halen = ETH_ALEN;
        address->sll_ifindex = ifr.ifr_ifindex;
        memcpy(address->sll_addr, mac_address, ETH_ALEN);

        if (ioctl(sockfd, SIOCGIFHWADDR, &ifr) < 0)
        {
            printf("[ERROR] Error getting MAC address of the interface to send on (SIOCGIFHWADDR)! %d | %s\n", errno, strerror(errno));
            return -1;
        }

        msg->header.SourceMAC.b[0] = ifr.ifr_hwaddr.sa_data[0];
        msg->header.SourceMAC.b[1] = ifr.ifr_hwaddr.sa_data[1];
        msg->header.SourceMAC.b[2] = ifr.ifr_hwaddr.sa_data[2];
        msg->header.SourceMAC.b[3] = ifr.ifr_hwaddr.sa_data[3];
        msg->header.SourceMAC.b[4] = ifr.ifr_hwaddr.sa_data[4];
        msg->header.SourceMAC.b[5] = ifr.ifr_hwaddr.sa_data[5];
        printf("Debuf print 1\n");
    }

    return 0;
}

namespace Connector
{
    class openConnector
	{
	public:
	
	
		openConnector() {
			void *publisherETF(void *arg);
			void *userApplicationPub(void *arg);
			static pthread_t threadCreation(UA_Int32 threadPriority, UA_Int32 coreAffinity, void *(*thread) (void *),
											char *applicationName, void *serverConfig);

			
		}
		
		static void stopHandler(int sign) {
			UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
			running = UA_FALSE;
		}
		
		static void nanoSecondFieldConversion(struct timespec *timeSpecValue) {
			/* Check if ns field is greater than '1 ns less than 1sec' */
			while (timeSpecValue->tv_nsec > (SECONDS -1)) {
				/* Move to next second and remove it from ns field */
				timeSpecValue->tv_sec  += SECONDS_INCREMENT;
				timeSpecValue->tv_nsec -= SECONDS;
			}

		}
		
	private:
		static UA_Double  cycleTimeMsec   = DEFAULT_CYCLE_TIME;
			static UA_Boolean consolePrint    = UA_FALSE;
			static UA_Int32   socketPriority  = DEFAULT_SOCKET_PRIORITY;
			static UA_Int32   pubPriority     = DEFAULT_PUB_SCHED_PRIORITY;
			static UA_Int32   userAppPriority = DEFAULT_USERAPPLICATION_SCHED_PRIORITY;
			static UA_Int32   pubSubCore      = DEFAULT_PUBSUB_CORE;
			static UA_Int32   userAppCore     = DEFAULT_USER_APP_CORE;
			static UA_Boolean useSoTxtime     = UA_TRUE;

			static UA_Double  userAppWakeupPercentage = 0.3;

			static UA_Double  pubWakeupPercentage     = 0.6;
			static UA_Boolean fileWrite = UA_FALSE;

			UA_Boolean          running = UA_TRUE;
			UA_UInt16           nsIdx = 0;

			UA_NodeId           connectionIdent;
			UA_NodeId           publishedDataSetIdent;
			UA_NodeId           writerGroupIdent;

			UA_Int32            *pubCounterData;
			UA_DataValue        *pubDataValueRT;

			UA_Int32	        *pressureData;
			UA_DataValue        *pressureValueRT;

			UA_Int32            *testData;
			UA_DataValue        *testDataValueRT;

			pthread_t           pubthreadID;
			struct timespec     dataModificationTime;

			pthread_t           userApplicationThreadID;

			typedef struct {
			UA_Server*                   ServerRun;
			} serverConfigStruct;

			typedef struct {
			UA_Server*                   server;
			void*                        data;
			UA_ServerCallback            callback;
			UA_Duration                  interval_ms;
			UA_UInt64*                   callbackId;
			} threadArg;
	};
	
	class Connector
    {
    public:
        Connector(bool inUse) : inUse(inUse)
        {
            outMsg = init_message();
            outMsg.data.DataSetMessage.DataKeyFrame = malloc(sizeof(SDMUADPDataKeyFrame_Machine5AchsCNC_1ms));
            buffer = (unsigned char *)malloc(1522); // to receive data
            memset(buffer, 0, 1522);
        }
        virtual void sendOneMessage() {};
        virtual SDMFrame *receiveOneMessage() { return nullptr; };

        bool inUse;
        RawMessage outMsg;
        SDMUADPDataKeyFrame_Machine5AchsCNC_1ms *machinePayload;
        unsigned char *buffer;
    };
    class TsnConnector : public Connector
    {
    public:
        TsnConnector(std::string interfaceName, ETHERNET_ADDRESS destinationMac, bool useTSN) : Connector(useTSN), interfaceName(interfaceName), destinationMac(destinationMac)
        {
            interfaceNameLength = interfaceName.length();
            socketHandle = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
            if (socketHandle < 0)
            {
                std::cout << "Error opening socket" << '\n';
            }
            auto rc = setsockopt(socketHandle, SOL_SOCKET, SO_BINDTODEVICE, interfaceName.c_str(), interfaceNameLength);

            if (rc != 0)
            {
                std::cout << "Error setting sockopts " << errno << ':' << strerror(errno) << '\n';
            }

            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            memcpy(ifr.ifr_name, interfaceName.c_str(), interfaceNameLength);
            if (ioctl(socketHandle, SIOCGIFINDEX, &ifr) == -1)
            {
                std::cout << "Error doing whatever this is." << '\n';
            }

            memset(&addressSend, 0, sizeof(addressSend));
            createEthernetAddrStruct(&addressSend, &outMsg, destinationMac.b, socketHandle, const_cast<char *>(interfaceName.c_str()));

            memset(&address, 0, sizeof(address));
            address.sll_family = AF_PACKET;
            address.sll_protocol = htons(ETH_P_ALL);
            address.sll_ifindex = ifr.ifr_ifindex;

            int ret = bind(socketHandle, (struct sockaddr *)&address, sizeof(address));
            if (ret < 0)
            {
                std::cout << "Error binding socket " << errno << ':' << strerror(errno) << '\n';
            }

            buffer = (unsigned char *)malloc(1522); // to receive data
            memset(buffer, 0, 1522);
            socklen = sizeof(address);
            std::cout << "Initialized  'tsn'\n";
        }

        void sendOneMessage() override
        {
            void *buf = (void *)&outMsg;

            size_t msg_data_size =
                sizeof(outMsg.header)                                     // 14
                + sizeof(outMsg.data.NetworkMessageHeader)                // 4
                + sizeof(outMsg.data.GroupHeader)                         // 11
                + sizeof(outMsg.data.DataSetMessage.DataSetMessageHeader) // 5
                + sizeof(SDMUADPDataKeyFrame_Machine5AchsCNC_1ms);        // 44

            struct msghdr msh;
            struct iovec iov;
            struct ifreq ifr;

            // Check if network interface for this socket is up
            strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ);
            if (ioctl(socketHandle, SIOCGIFFLAGS, &ifr) < 0)
            {
                printf("[PID %d] ERROR: Requesting SIOCGIFFLAGS returned: %s\n", getpid(), strerror(errno));
            }
            else
            {
                if (!(ifr.ifr_flags & IFF_RUNNING))
                {
                    printf("[PID %d] WARNING: Sending on interface which is not running!\n", getpid());
                }
            }

            iov.iov_base = buf;
            iov.iov_len = msg_data_size;

            memset(&msh, 0, sizeof(msh));
            msh.msg_name = (void *)&addressSend;
            msh.msg_namelen = sizeof(addressSend);
            msh.msg_iov = &iov;
            msh.msg_iovlen = 1;

            // tx time specific settings

            msh.msg_control = 0;
            msh.msg_controllen = 0;

            // send packet
            ssize_t s = sendmsg(socketHandle, &msh, 0);
            // printf("%ld bytes sent\n", s);
        }

        SDMFrame *receiveOneMessage() override
        {
            ssize_t buflen;

            buflen = recvfrom(socketHandle, buffer, 1522, MSG_DONTWAIT, (struct sockaddr *)&address, (socklen_t *)&socklen);
            if (buflen < 0)
            {
                // printf("[ERROR] Failure receiving data! %d | %s\n", errno, strerror(errno));
                return nullptr;
            }

            SDMFrame *sdmFrame = (SDMFrame *)buffer;

            if (ntohs(sdmFrame->header.EtherType) == 0xB62C)
            {
                if (sdmFrame->data.NetworkMessageHeader.PublisherId == PUBLISHERID_MACHINE_5ACHSCNC)
                {
                    SDMUADPDataKeyFrame_Machine5AchsCNC *machine = (SDMUADPDataKeyFrame_Machine5AchsCNC *)&sdmFrame->data.DataSetMessage.DataKeyFrame;

                    if (sdmFrame->data.GroupHeader.WriterGroupId == WRITERGROUPID_1MS)
                    {
                        SDMUADPDataKeyFrame_Machine5AchsCNC_IST_1ms *data = (SDMUADPDataKeyFrame_Machine5AchsCNC_IST_1ms *)&machine->payload;
                    }
                }

                return sdmFrame;
            }
            else
            {
                sdmFrame = nullptr;
            }
            return nullptr;
        }

    private:
        int socketHandle;
        std::string interfaceName;
        size_t interfaceNameLength;
        int socklen;
        struct sockaddr_ll address;
        struct sockaddr_ll addressSend;
        ETHERNET_ADDRESS destinationMac;
    };

    class TcpConnector : public Connector
    {
    public:
        TcpConnector(std::string ip, in_port_t port, bool useTCP) : Connector(useTCP)
        {
            int connfd;
            struct sockaddr_in servaddr, cli;

            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1)
            {
                std::cout << "socket creation failed...\n";
                inUse = false;
                return;
            }
            else
            {
                std::cout << "Socket successfully created..\n";
            }
            bzero(&servaddr, sizeof(servaddr));

            // assign IP, PORT
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr(ip.c_str());
            servaddr.sin_port = htons(port);

            if(!inUse) return;
            
            // connect the client socket to server socket
            if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) != 0)
            {
                std::cout << "connection with the server failed...\n";
                inUse = false;
            }
            else {
                std::cout << "connected to the server..\n";
            }
            std::cout << "Initialized  'TCP'\n";
        }

        void sendOneMessage() override
        {
            void *buf = (void *)&outMsg;

            size_t msg_data_size = sizeof(SDMEthernetHeader) + sizeof(SDMUADPFrame) - sizeof(void*) + sizeof(SDMUADPDataKeyFrame_Machine5AchsCNC_1ms);        // 44

            // Check if network interface for this socket is up
            if (int ret = send(sockfd, buf, msg_data_size, 0) <= 0) {
                std::cout << "Failed sending message " << ret << '\n';
            }
        }

        SDMFrame *receiveOneMessage() override
        {
            return nullptr;
        }

    private:
        unsigned char *buffer;
        int sockfd;
    };
}

#endif
