// 인터럽트/async 분기 플래그그
#define USE_INTERRUPTS      1
#define TEST_STRING     "{\"test\":\"wifi_test\"}"

#define SD_WIFI_SUCCESS 0
#define SD_WIFI_FAIL    1

#define AT_OK           "OK"
#define AT_ERROR        "ERROR"
#define AT_READY        "ready"

// #define MSG_SIZE            2048
#define MSG_SIZE            128
#define MAX_WIFI_NETWORKS   30
#define MAX_SSID_LENGTH     64
#define MAX_CMD_LENGTH      128

#define DEFAULT_WIFI_SSID   "Test"
#define DEFAULT_WIFI_PWD    "qwer1234"

#define DEFAULT_URL         "http://test.com"
#define ROUTE_EXAMPLE       "/example"
#define ROUTE_TEST          "/test"

#define BODY_WAIT_INTERVAL  60
#define WAIT_RESPONSE_TIME  60

// AT Command strings
#define AT_DEFAULT          "AT"
#define AT_STASCAN          "AT+QWSCAN"
#define AT_STAAPINFO        "AT+QSTAAPINFO"
#define AT_STAST            "AT+QSTAST"
#define AT_STASTOP          "AT+QSTASTOP"
#define AT_GETIP            "AT+QGETIP"
#define AT_LOPOWER          "AT+QLOPOWER"
#define AT_DEEPSLEEP        "AT+QDEEPSLEEP"
#define AT_HTTP_CFG         "AT+QHTTPCFG"
#define AT_HTTP_POST        "AT+QHTTPPOST"
#define AT_HTTP_READ        "AT+QHTTPREAD"

// HTTP sub-commands
#define SUB_HEADER      "header"
#define SUB_URL         "url"
#define SUB_TYPE        "content_type"
#define SUB_AGENT       "user_agent"

/* WiFi network structure */
struct wifi_network {
    char ssid[MAX_SSID_LENGTH];
    char security[32];
    int rssi;
    // char mac[18];
    // int channel;
};

typedef enum {
    CMD_NULL = 0,
    CMD_AT_IDX,         // "AT"
    CMD_STASCAN_IDX,       // WiFi scan
    CMD_STACONN_IDX,       // WiFi connect
    CMD_STAST_IDX,       // Status
    CMD_HTTP_OUT_IDX,   // HTTP output
    CMD_HTTP_TYPE_IDX,  // Content type
    CMD_HTTP_AGENT_IDX, // User agent
    CMD_HTTP_URL_IDX,   // URL
    CMD_HTTP_POST_IDX,  // POST
    CMD_MAX
} AT_CMD_IDX;