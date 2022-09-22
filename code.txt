#define _GNU_SOURCE

/* For thread operations */
#include <pthread.h>

#include <open62541/server.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/types_generated.h>
#include <open62541/plugin/pubsub_ethernet.h>

#include "ua_pubsub.h"
#include "open62541/namespace_example_publisher_generated.h"

/* to find load of each thread
 * ps -L -o pid,pri,%cpu -C pubsub_nodeset_rt_publisher */

/* Configurable Parameters */
/* Cycle time in milliseconds */
#define             DEFAULT_CYCLE_TIME                    0.25
/* Qbv offset */
#define             QBV_OFFSET                            25 * 1000
#define             DEFAULT_SOCKET_PRIORITY               3
#define             PUBLISHER_ID                          2234
#define             WRITER_GROUP_ID                       101
#define             DATA_SET_WRITER_ID                    62541
#define             PUBLISHING_MAC_ADDRESS                "opc.eth://01-00-5E-7F-00-01:8.3"
#define             PORT_NUMBER                           62541

/* Non-Configurable Parameters */
/* Milli sec and sec conversion to nano sec */
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

/* Below mentioned parameters can be provided as input using command line arguments
 * If user did not provide the below mentioned parameters as input through command line
 * argument then default value will be used */
static UA_Double  cycleTimeMsec   = DEFAULT_CYCLE_TIME;
static UA_Boolean consolePrint    = UA_FALSE;
static UA_Int32   socketPriority  = DEFAULT_SOCKET_PRIORITY;
static UA_Int32   pubPriority     = DEFAULT_PUB_SCHED_PRIORITY;
static UA_Int32   userAppPriority = DEFAULT_USERAPPLICATION_SCHED_PRIORITY;
static UA_Int32   pubSubCore      = DEFAULT_PUBSUB_CORE;
static UA_Int32   userAppCore     = DEFAULT_USER_APP_CORE;
static UA_Boolean useSoTxtime     = UA_TRUE;

/* User application Pub will wakeup at the 30% of cycle time and handles the */
/* user data write in Information model */
/* First 30% is left for subscriber for future use*/
static UA_Double  userAppWakeupPercentage = 0.3;
/* Publisher will sleep for 60% of cycle time and then prepares the */
/* transmission packet within 40% */
/* after some prototyping and analyzing it */
static UA_Double  pubWakeupPercentage     = 0.6;
static UA_Boolean fileWrite = UA_FALSE;

/* If the Hardcoded publisher MAC addresses need to be changed,
 * change PUBLISHING_MAC_ADDRESS
 */

/* Set server running as true */
UA_Boolean          running = UA_TRUE;
UA_UInt16           nsIdx = 0;
/* Variables corresponding to PubSub connection creation,
 * published data set and writer group */
UA_NodeId           connectionIdent;
UA_NodeId           publishedDataSetIdent;
UA_NodeId           writerGroupIdent;
/* Variables for counter data handling in address space */
UA_UInt64           *pubCounterData;
UA_DataValue        *pubDataValueRT;
/* Variables for counter data handling in address space */
UA_Double           *pressureData;
UA_DataValue        *pressureValueRT;

/* File to store the data and timestamps for different traffic */
FILE               *fpPublisher;
char               *fileName      = "publisher_T1.csv";
/* Array to store published counter data */
UA_UInt64           publishCounterValue[MAX_MEASUREMENTS];
UA_Double           pressureValues[MAX_MEASUREMENTS];
size_t              measurementsPublisher  = 0;
/* Array to store timestamp */
struct timespec     publishTimestamp[MAX_MEASUREMENTS];

/* Thread for publisher */
pthread_t           pubthreadID;
struct timespec     dataModificationTime;

/* Thread for user application*/
pthread_t           userApplicationThreadID;

typedef struct {
UA_Server*                   ServerRun;
} serverConfigStruct;

/* Structure to define thread parameters */
typedef struct {
UA_Server*                   server;
void*                        data;
UA_ServerCallback            callback;
UA_Duration                  interval_ms;
UA_UInt64*                   callbackId;
} threadArg;

/* Publisher thread routine for ETF */
void *publisherETF(void *arg);
/* User application thread routine */
void *userApplicationPub(void *arg);
/* To create multi-threads */
static pthread_t threadCreation(UA_Int32 threadPriority, UA_Int32 coreAffinity, void *(*thread) (void *),
                                char *applicationName, void *serverConfig);

/* Stop signal */
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = UA_FALSE;
}
