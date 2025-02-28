#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <stdio.h>
#include <string.h>
// #include <strings.h>  // For string manipulation functions
#include <stdlib.h>

#include "fc41d_wifi.h"

K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 32, 4);

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_NODELABEL(uart1)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

static struct wifi_network networks[MAX_WIFI_NETWORKS];
static int network_count = 0;
/* receive buffer used in UART callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;
static int send_cmd = CMD_NULL;

static int init_step = CMD_HTTP_OUT_IDX;
static int post_step = CMD_HTTP_URL_IDX;

// static method
static void send_uart(char *buf);

void set_send_cmd(int cmd)
{
    send_cmd = cmd;
}

int get_send_cmd(void)
{
    return send_cmd;
}

static void parse_wifi_info(char *line)
{
    // Skip to the start of data after "+QWSCAN:"
    char *ptr = strstr(line, "+QWSCAN:");
    if (!ptr) return;
    ptr += 8;  // Skip "+QWSCAN:"

    struct wifi_network *net = &networks[network_count];
    memset(net, 0, sizeof(struct wifi_network));

    // Parse SSID (between quotes)
    if (*ptr == '"') {
        ptr++;  // Skip first quote
        char *end = strchr(ptr, '"');
        if (end) {
            int len = end - ptr;
            if (len > 0 && len < MAX_SSID_LENGTH) {
                strncpy(net->ssid, ptr, len);
                net->ssid[len] = '\0';
                ptr = end + 1;  // Skip closing quote
            }
        }
    }

    // Skip to security (after comma)
    ptr = strchr(ptr, ',');
    if (ptr) {
        ptr++;  // Skip comma
        char *end = strchr(ptr, ',');
        if (end) {
            int len = end - ptr;
            if (len > 0 && len < sizeof(net->security)) {
                strncpy(net->security, ptr, len);
                net->security[len] = '\0';
                ptr = end + 1;  // Skip comma
            }
        }
    }

    // Parse RSSI
    if (ptr) {
        net->rssi = atoi(ptr);
    }

    // Store if we got valid data
    if (net->ssid[network_count] && net->security[network_count]) {
        network_count = (network_count + 1) % MAX_WIFI_NETWORKS;
    }
}

void send_next_init_command(void)
{
    // static int init_step = CMD_HTTP_OUT_IDX;
    printk("init_step: %d\n", init_step);

    switch (init_step) {
        case CMD_HTTP_OUT_IDX:
            send_uart(AT_HTTP_CFG "=\"response/output\",1\r\n");
            break;

        case CMD_HTTP_TYPE_IDX:
            send_uart(AT_HTTP_CFG "=\"" SUB_HEADER "\",\"" SUB_TYPE "\",\"application/json\"\r\n");
            break;

        case CMD_HTTP_AGENT_IDX:
            send_uart(AT_HTTP_CFG "=\"" SUB_HEADER "\",\"" SUB_AGENT "\",\"WIFI_TEST\"\r\n");
            break;

        default:
            return;
    }

    set_send_cmd(init_step);
    init_step++;
}

void send_next_post_command(void)
{
    // static int post_step = CMD_HTTP_URL_IDX;
    printk("post_step: %d\n", post_step);

    switch (post_step) {
        case CMD_HTTP_URL_IDX:
            send_uart(AT_HTTP_CFG "=\"" SUB_URL "\",\"" DEFAULT_URL ROUTE_EXAMPLE ROUTE_TEST "\"\r\n");
            break;

        case CMD_HTTP_POST_IDX:
            send_uart(AT_HTTP_POST "=31,60,60\r\n");
            break;

        default:
            return;
    }

    set_send_cmd(post_step);
    post_step++;
}

void parse_msg(char *msg)
{
    printk("Received[%d]: %s\n", get_send_cmd(), msg);
    // for(int i = 0; i<strlen(msg); i++){
    //     printk( "%x ", msg[i]);
    // }
    // printk("\n");

    char send_buf[MAX_CMD_LENGTH];
    // int resp = 0;

    if(strcmp(msg, AT_READY) == 0){
        printk("WiFi modem ready\n");
        init_step = CMD_HTTP_OUT_IDX;
        post_step = CMD_HTTP_URL_IDX;
        send_uart("AT\r\n");
        set_send_cmd(CMD_AT_IDX);
        return;
    }

    if(strcmp(msg, AT_ERROR) == 0){
        // ERROR시 WIFI 모뎀 리셋
        send_uart("AT+QRST\r\n");
        return;
    }

    if (strstr(msg, "+QWSCAN:")) {
        // Create a safe copy of the message
        printk("Parsing WiFi info\n");
        char msg_copy[MSG_SIZE];
        memset(msg_copy, 0, sizeof(msg_copy));
        strncpy(msg_copy, msg, sizeof(msg_copy) - 1);
        parse_wifi_info(msg_copy);
        return;
    }

    switch (get_send_cmd()) {
        case CMD_AT_IDX:
            if(strcmp(msg, AT_OK) != 0){
                printk("Failed to initialize\n");
                send_uart("AT+QRST\r\n");
                return;
            }
            send_uart(AT_STASCAN "\r\n");
            set_send_cmd(CMD_STASCAN_IDX);
            return;

        case CMD_STASCAN_IDX:
            // Print all collected networks
            printk("\nFound %d networks:\n", network_count);

            // 해당 동작은 registered(등록된 기기)의 경우 wifi 자동 연결시
            for (int i = 0; i < network_count; i++) {
                // k_sleep(K_MSEC(1));
                printk("chk ssid: %s\n", networks[i].ssid);
                if(strcmp(networks[i].ssid, DEFAULT_WIFI_SSID) == 0){
                    printk("Network %d:\n", i + 1);
                    printk("  SSID: %s\n", networks[i].ssid);
                    printk("  Security: %s\n", networks[i].security);
                    printk("  RSSI: %d\n", networks[i].rssi);
                    // printk("  MAC: %s\n", networks[i].mac);
                    // printk("  Channel: %d\n", networks[i].channel);

                    memset(send_buf, 0, sizeof(send_buf));
                    snprintf(send_buf, sizeof(send_buf), "%s=%s,%s\r\n",
                            AT_STAAPINFO, DEFAULT_WIFI_SSID, DEFAULT_WIFI_PWD);
                    send_uart(send_buf);
                    // send_cmd = AT_CMD_QSTAAPINFO;
                    set_send_cmd(CMD_STACONN_IDX);
                    return;
                }
            }
            break;

        case CMD_STACONN_IDX:
            if(strstr(msg, "GOT_IP") == NULL){
                printk("wait for IP address\n");
                return;
            }
            printk("WiFi connection successful\n");
            send_next_init_command();  // HTTP 설정 초기화 시작
            return;

        case CMD_STAST_IDX:
            printk("QSTAST command successful\n");
            break;

        case CMD_HTTP_OUT_IDX:
        case CMD_HTTP_TYPE_IDX:
            if(strcmp(msg, AT_OK) == 0) {
                printk("Command successful\n");
                send_next_init_command();  // 다음 HTTP 설정
            }
            return;

        case CMD_HTTP_AGENT_IDX:
            if(strcmp(msg, AT_OK) == 0) {
                printk("HTTP configuration complete\n");
                send_next_post_command();  // POST 명령어 시작
            }
            return;

        case CMD_HTTP_URL_IDX:
            if(strcmp(msg, AT_OK) == 0) {
                memset(send_buf, 0, sizeof(send_buf));
                snprintf(send_buf, sizeof(send_buf), "%s=%d,%d,%d\r\n",
                        AT_HTTP_POST, strlen(TEST_STRING), BODY_WAIT_INTERVAL, WAIT_RESPONSE_TIME);
                send_uart(send_buf);
                set_send_cmd(CMD_HTTP_POST_IDX);
            }
            return;

        case CMD_HTTP_POST_IDX:
            if(strcmp(msg, "CONNECT") == 0){
                printk("HTTP POST command successful\n");
                memset(send_buf, 0, sizeof(send_buf));
                snprintf(send_buf, sizeof(send_buf), "%s\r\n", TEST_STRING);
                send_uart(send_buf);
                return;
            }
            break;
        default:
            break;
    }
    // send_cmd = AT_CMD_NULL;
    set_send_cmd(CMD_NULL);
}

#if USE_INTERRUPTS
void uart_read_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    uart_irq_rx_disable(uart_dev);
    /* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos -1] = '\0';
            rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
    uart_irq_rx_enable(uart_dev);
}
#else
/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void uart_read_cb(const struct device *dev, struct uart_event *evt, void *user_data)
{
    switch (evt->type) {
        case UART_TX_DONE:
            printk("TX_DONE\n");
            break;

        case UART_RX_RDY:
            if (rx_buf_pos + evt->data.rx.len < MSG_SIZE) {
                printk("RX_RDY: %d bytes\n", evt->data.rx.len);
                printk("rx_buf_pos[%d] evt->data.rx.offset[%d]\n", rx_buf_pos, evt->data.rx.offset);

                // 수신된 데이터를 버퍼에 복사
                memcpy(rx_buf + rx_buf_pos, evt->data.rx.buf + evt->data.rx.offset,
                       evt->data.rx.len);
                rx_buf_pos += evt->data.rx.len;
                rx_buf[rx_buf_pos] = '\0';  // 널 문자 추가 (디버깅 용도로 사용 가능)
            } else {
                // 버퍼 오버플로우 방지
                printk("Buffer overflow, resetting buffer\n");
                rx_buf_pos = 0;
                memset(rx_buf, 0, sizeof(rx_buf));
            }
            break;

        case UART_RX_BUF_REQUEST:
            printk("RX_BUF_REQUEST\n");
            break;

        default:
            break;
    }
}

void process_rx_data(void)
{
    if (rx_buf_pos > 0) {
        // 버퍼에 저장된 데이터를 출력
        for (int i = 0; i < rx_buf_pos; i++) {
            printk("%c", rx_buf[i]);
        }

        // 출력 후 버퍼 초기화
        rx_buf_pos = 0;
        memset(rx_buf, 0, sizeof(rx_buf));
    }
}
#endif

static void send_uart(char *buf)
{
    int msg_len = strlen(buf);
    printk("Sending %d bytes: %s", msg_len, buf);

#if USE_INTERRUPTS
    for (int i = 0; i < msg_len; i++) {
        uart_poll_out(uart_dev, buf[i]);
    }
#else
    int ret = uart_tx(uart_dev, buf, msg_len, 1000);
    if (ret < 0) {
        printk("Failed to send UART data: %d\n", ret);
    }
#endif
}

int main(void)
{
    char recv_buf[MSG_SIZE];

    if (!device_is_ready(uart_dev)) {
        printk("UART device not found!");
        return 0;
    }

    // UART 설정을 명시적으로 구성
    struct uart_config cfg;
    cfg.baudrate = 115200;
    cfg.parity = UART_CFG_PARITY_NONE;
    cfg.stop_bits = UART_CFG_STOP_BITS_1;
    cfg.data_bits = UART_CFG_DATA_BITS_8;
    cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

    if (uart_configure(uart_dev, &cfg) < 0) {
        printk("Failed to configure UART\n");
        return -1;
    }

#if USE_INTERRUPTS
    int ret = uart_irq_callback_set(uart_dev, uart_read_cb);
#else
    int ret = uart_callback_set(uart_dev, uart_read_cb, NULL);
#endif
    if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}

#if USE_INTERRUPTS
    uart_irq_rx_enable(uart_dev);
#else
    uart_rx_enable(uart_dev, rx_buf, MSG_SIZE, 1000);
#endif

    // 현재 UART 설정 확인
    int err = uart_config_get(uart_dev, &cfg);
    if (err == 0) {
        printk("UART Configuration:\n");
        printk("  Baudrate: %d\n", cfg.baudrate);
        printk("  Parity: %d\n", cfg.parity);
        printk("  Stop bits: %d\n", cfg.stop_bits);
        printk("  Data bits: %d\n", cfg.data_bits);
        printk("  Flow control: %d\n", cfg.flow_ctrl);
    } else {
        printk("Failed to get UART config: %d\n", err);
    }

    // Start initialization sequence
    // send_next_init_command();

    send_uart(AT_DEFAULT "\r\n");
    set_send_cmd(CMD_AT_IDX);

#if USE_INTERRUPTS
    while(k_msgq_get(&uart_msgq, &recv_buf, K_FOREVER) == 0){
        // printk("Received: %s\n", recv_buf);
        if(recv_buf[0] == '\0'){
            continue;
        }
        parse_msg(recv_buf);
    }
#else
    while(1){
        process_rx_data();
        k_sleep(K_MSEC(10));
    }
#endif

	return 0;
}
