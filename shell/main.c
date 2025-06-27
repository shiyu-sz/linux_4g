
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

static void usage(void)
{
    printf("pppd_test [-h] \n");
    printf("  -h : 帮助 \n");
    printf("  -c : 拨号 \n");
    printf("  -d : 断开拨号 \n");
    printf("  -p : 检查系统是否联网 \n");
    printf("  -g : 获得当前使用的网卡 \n");
}

// 判断系统是否联网
int is_online() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(80);
    serv_addr.sin_addr.s_addr = inet_addr("157.148.69.186"); 
    
    // 设置超时(1秒)
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    int conn = connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    close(sock);
    
    return (conn == 0) ? 1 : 0;
}

// 判断系统使用的网卡
int get_network_scard(char *buffer, int len) {
    FILE *fp = popen("route -n | awk '$1 == \"0.0.0.0\" {print $8; exit}'", "r");
    if (!fp) {
        perror("popen() failed");
        return 1;
    }

    if(fgets(buffer, len, fp) != NULL) {
        printf("当前使用的网卡: %s\n", buffer);
    }

    int status = pclose(fp);
    if (status == -1) {
        perror("pclose() failed");
    } else {
        printf("命令退出状态: %d\n", WEXITSTATUS(status));
    }
    return 0;
}

int main(int argc, char *argv[])
{
    int c;

	while ((c = getopt(argc, argv, "hcdpg")) != -1) {
        switch (c) {
        case 'h':
            usage();
            break;
        case 'c':
            {
                int status = system("pppd call quectel-ppp &");
                if (status == -1) {
                    printf("创建进程失败\n");
                } else if (WIFEXITED(status)) {
                    printf("退出码: %d\n", WEXITSTATUS(status));
                }
            }
            break;
        case 'd':
            {
                int status = system("/etc/ppp/peers/quectel-ppp-kill");
                if (status == -1) {
                    printf("创建进程失败\n");
                } else if (WIFEXITED(status)) {
                    printf("退出码: %d\n", WEXITSTATUS(status));
                }
            }
            break;
        case 'p':
            {
                if (is_online()) {
                    printf("系统已联网\n");
                } else {
                    printf("系统未联网\n");
                }
            }
            break;
        case 'g':
            {
                char buff[128];
                get_network_scard(buff, 128);
            }
            break;
        default:
            printf("Error: invalid options. \n");
            usage();
            return -1;
        }
    }
}