
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

#define SERIAL_PORT "/dev/ttyUSB4"
#define BAUDRATE B115200

// 发送AT指令并检查响应
int send_at_command(int fd, const char *cmd, const char *expected_resp, int timeout_sec) {
    char buffer[256];
    int len, n;
    
    printf("Send: %s\n", cmd);
    // 发送指令
    len = strlen(cmd);
    if (write(fd, cmd, len) != len) {
        perror("write");
        return -1;
    }
    tcdrain(fd);  // 等待数据完全发送
    
    // 读取响应
    memset(buffer, 0, sizeof(buffer));
    int total_bytes = 0;
    time_t start, curr;
    time(&start);
    
    while (curr - start < timeout_sec) {
        n = read(fd, buffer + total_bytes, sizeof(buffer) - total_bytes - 1);
        if (n > 0) {
            total_bytes += n;
            buffer[total_bytes] = '\0';
            
            // 检查是否包含预期响应
            if (strstr(buffer, expected_resp) != NULL) {
                printf("Response: %s\n", buffer);
                return 0;  // 成功
            }
        } else if (n < 0 && errno != EAGAIN) {
            perror("read");
            break;
        }
        usleep(100000);  // 100ms延迟
        time(&curr);
    }
    
    fprintf(stderr, "Timeout or error. Received: %s\n", buffer);
    return -1;
}

int main() {
    int fd;
    struct termios tty;

    int status = system("pppd /dev/ttyUSB4 115200 user \"test\" password \"test\"");

    // pppd $QL_DEVNAME 115200 user "$QL_USER" password "$QL_PASSWORD"

    // 检查返回值
    if (status == -1) {
        printf("创建进程失败\n");
    } else if (WIFEXITED(status)) {
        printf("退出码: %d\n", WEXITSTATUS(status));
    }

    // 1. 打开串口
    fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    
    // 2. 配置串口
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    cfsetospeed(&tty, BAUDRATE);
    cfsetispeed(&tty, BAUDRATE);
    
    tty.c_cflag &= ~PARENB;    // 无奇偶校验
    tty.c_cflag &= ~CSTOPB;    // 1位停止位
    tty.c_cflag &= ~CSIZE;     // 清除数据位掩码
    tty.c_cflag |= CS8;        // 8位数据位
    tty.c_cflag &= ~CRTSCTS;   // 禁用硬件流控
    tty.c_cflag |= CREAD | CLOCAL;  // 启用接收
    
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // 原始模式
    tty.c_oflag &= ~OPOST;     // 原始输出
    
    tty.c_cc[VMIN] = 1;        // 至少读1字符
    tty.c_cc[VTIME] = 5;       // 0.5秒超时
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        exit(EXIT_FAILURE);
    }
    
    tcflush(fd, TCIOFLUSH);  // 清空缓冲区
    
    // 3. 发送AT指令序列
    if (send_at_command(fd, "AT\r", "OK", 2) < 0) goto error;
    if (send_at_command(fd, "ATE0\r", "OK", 2) < 0) goto error;
    if (send_at_command(fd, "ATI;+CSUB;+CSQ;+CPIN?;+COPS?;+CGREG?;&D2\r", "OK", 2) < 0) goto error;
    if (send_at_command(fd, "AT+CGDCONT=1,\"IP\",\"3gnet\",,0,0\r", "OK", 2) < 0) goto error; // 设置APN
    
    // 发起拨号
    if (send_at_command(fd, "ATD*99#\r", "CONNECT", 15) < 0) goto error;
    
    printf("Dial-up successful! PPP connection established.\n");
    close(fd);
    
    while (1)
    {
        sleep(1);  // 保持程序运行
    }
    

error:
    fprintf(stderr, "Dial-up failed\n");
    close(fd);
    return EXIT_FAILURE;
}